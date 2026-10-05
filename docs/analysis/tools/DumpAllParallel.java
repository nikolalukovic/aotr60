// Parallel variant of DumpAll.java: dumps summary, imports, exports, functions, call graph,
// strings, symbols, float-constant references, full disassembly and decompiled C.
// Args: <outDir> [decompileTimeoutSeconds] [threads]
//@category Export

import java.io.File;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressIterator;
import ghidra.program.model.data.StringDataInstance;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.ExternalLocation;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceManager;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import ghidra.program.model.symbol.SymbolTable;
import ghidra.program.util.DefinedDataIterator;
import ghidra.util.task.TaskMonitor;

public class DumpAllParallel extends GhidraScript {

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            printerr("Usage: DumpAllParallel.java <outDir> [decompileTimeoutSeconds] [threads]");
            return;
        }
        File outDir = new File(args[0]);
        int timeout = args.length > 1 ? Integer.parseInt(args[1]) : 60;
        int threads = args.length > 2 ? Integer.parseInt(args[2]) : 16;
        outDir.mkdirs();

        writeSummary(new File(outDir, "summary.txt"));
        writeImports(new File(outDir, "imports.txt"));
        writeExports(new File(outDir, "exports.tsv"));
        writeFunctions(new File(outDir, "functions.tsv"), new File(outDir, "callgraph.tsv"));
        writeStrings(new File(outDir, "strings.tsv"));
        writeSymbols(new File(outDir, "symbols.tsv"));
        writeFloatRefs(new File(outDir, "floatrefs.tsv"));
        writeListing(new File(outDir, "listing.asm"));
        println("PHASE metadata done");
        writeDecompiled(new File(outDir, "decompiled.c"), timeout, threads);
        println("DUMP_DONE " + outDir.getAbsolutePath());
    }

    private static PrintWriter open(File f) throws Exception {
        return new PrintWriter(f, StandardCharsets.UTF_8);
    }

    private void writeSummary(File f) throws Exception {
        try (PrintWriter w = open(f)) {
            w.println("Name:       " + currentProgram.getName());
            w.println("Path:       " + currentProgram.getExecutablePath());
            w.println("Format:     " + currentProgram.getExecutableFormat());
            w.println("Language:   " + currentProgram.getLanguageID());
            w.println("Compiler:   " + currentProgram.getCompilerSpec().getCompilerSpecID());
            w.println("Image base: " + currentProgram.getImageBase());
            w.println("MD5:        " + currentProgram.getExecutableMD5());
            w.println("SHA256:     " + currentProgram.getExecutableSHA256());
            w.println("Functions:  " + currentProgram.getFunctionManager().getFunctionCount());
            w.println();
            w.println("Memory blocks:");
            for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
                w.printf("  %-12s %s-%s %10d %s%s%s%n", b.getName(), b.getStart(), b.getEnd(),
                    b.getSize(), b.isRead() ? "r" : "-", b.isWrite() ? "w" : "-",
                    b.isExecute() ? "x" : "-");
            }
        }
    }

    private void writeImports(File f) throws Exception {
        List<String> imports = new ArrayList<>();
        FunctionIterator it = currentProgram.getFunctionManager().getExternalFunctions();
        while (it.hasNext()) {
            Function ext = it.next();
            StringBuilder sb = new StringBuilder(externalName(ext));
            int n = 0;
            for (Reference r : getReferencesTo(ext.getEntryPoint())) {
                Function user = getFunctionContaining(r.getFromAddress());
                sb.append(n == 0 ? "\t" : ", ");
                sb.append(user != null ? user.getName(true) + "@" + user.getEntryPoint()
                        : r.getFromAddress().toString());
                if (++n >= 40) {
                    break;
                }
            }
            imports.add(sb.toString());
        }
        Collections.sort(imports, String.CASE_INSENSITIVE_ORDER);
        try (PrintWriter w = open(f)) {
            imports.forEach(w::println);
        }
    }

    private void writeExports(File f) throws Exception {
        SymbolTable st = currentProgram.getSymbolTable();
        try (PrintWriter w = open(f)) {
            w.println("address\tname");
            AddressIterator it = st.getExternalEntryPointIterator();
            while (it.hasNext()) {
                Address a = it.next();
                Symbol s = st.getPrimarySymbol(a);
                w.println(a + "\t" + (s != null ? s.getName(true) : "?"));
            }
        }
    }

    private void writeFunctions(File funcs, File calls) throws Exception {
        try (PrintWriter fw = open(funcs); PrintWriter cw = open(calls)) {
            fw.println("address\tname\tsize\tcallers\tcallees\tthunk\tsignature");
            cw.println("caller_address\tcaller\tcallee_address\tcallee");
            for (Function fn : currentProgram.getFunctionManager().getFunctions(true)) {
                monitor.checkCancelled();
                Set<Function> callers = fn.getCallingFunctions(monitor);
                Set<Function> callees = fn.getCalledFunctions(monitor);
                fw.println(fn.getEntryPoint() + "\t" + fn.getName(true) + "\t" +
                    fn.getBody().getNumAddresses() + "\t" + callers.size() + "\t" +
                    callees.size() + "\t" + (fn.isThunk() ? "yes" : "") + "\t" +
                    fn.getPrototypeString(false, false));
                for (Function callee : callees) {
                    String name = callee.isExternal() ? externalName(callee) : callee.getName(true);
                    cw.println(fn.getEntryPoint() + "\t" + fn.getName(true) + "\t" +
                        callee.getEntryPoint() + "\t" + name);
                }
            }
        }
    }

    private void writeStrings(File f) throws Exception {
        ReferenceManager rm = currentProgram.getReferenceManager();
        try (PrintWriter w = open(f)) {
            w.println("address\tvalue\treferenced_from");
            for (Data d : DefinedDataIterator.definedStrings(currentProgram)) {
                monitor.checkCancelled();
                String value = StringDataInstance.getStringDataInstance(d).getStringValue();
                if (value == null) {
                    continue;
                }
                Set<String> users = new LinkedHashSet<>();
                for (Reference r : rm.getReferencesTo(d.getAddress())) {
                    Function user = getFunctionContaining(r.getFromAddress());
                    users.add(user != null ? user.getName(true) + "@" + user.getEntryPoint()
                            : r.getFromAddress().toString());
                    if (users.size() >= 10) {
                        break;
                    }
                }
                w.println(d.getAddress() + "\t" + escape(value) + "\t" + String.join(", ", users));
            }
        }
    }

    // Every non-default symbol (RTTI names, vftables, imported/analysis labels).
    private void writeSymbols(File f) throws Exception {
        SymbolTable st = currentProgram.getSymbolTable();
        try (PrintWriter w = open(f)) {
            w.println("address\ttype\tsource\tname");
            SymbolIterator it = st.getAllSymbols(true);
            while (it.hasNext()) {
                Symbol s = it.next();
                if (s.getSource() == SourceType.DEFAULT || s.isExternal()) {
                    continue;
                }
                w.println(s.getAddress() + "\t" + s.getSymbolType() + "\t" + s.getSource() + "\t" +
                    escape(s.getName(true)));
            }
        }
    }

    // Floating-point loads/stores/arith against static memory, with the value at that address.
    private void writeFloatRefs(File f) throws Exception {
        Listing listing = currentProgram.getListing();
        Memory mem = currentProgram.getMemory();
        try (PrintWriter w = open(f)) {
            w.println("instr_address\tfunction\tmnemonic\ttarget\tsize\tvalue\tblock");
            for (Instruction ins : listing.getInstructions(true)) {
                monitor.checkCancelled();
                String m = ins.getMnemonicString();
                boolean fp = m.startsWith("F") || m.endsWith("SS") || m.endsWith("SD") ||
                    m.startsWith("CVT") || m.startsWith("UCOMIS") || m.startsWith("COMIS");
                if (!fp) {
                    continue;
                }
                for (Reference r : ins.getReferencesFrom()) {
                    if (!r.isMemoryReference()) {
                        continue;
                    }
                    Address t = r.getToAddress();
                    MemoryBlock b = mem.getBlock(t);
                    if (b == null || b.isExecute() || !b.isInitialized()) {
                        continue;
                    }
                    int op = r.getOperandIndex();
                    String rep = op >= 0 ? ins.getDefaultOperandRepresentation(op) : "";
                    int size;
                    String value;
                    try {
                        if (rep.contains("qword") || m.endsWith("SD")) {
                            size = 8;
                            value = Double.toString(Double.longBitsToDouble(mem.getLong(t)));
                        }
                        else if (rep.contains("tword")) {
                            size = 10;
                            value = "?";
                        }
                        else if (rep.contains("word ptr") && !rep.contains("dword")) {
                            size = 2;
                            value = Short.toString(mem.getShort(t));
                        }
                        else {
                            size = 4;
                            int bits = mem.getInt(t);
                            value = (m.startsWith("FI") ? Integer.toString(bits)
                                    : Float.toString(Float.intBitsToFloat(bits)) + "|0x" +
                                        Integer.toHexString(bits));
                        }
                    }
                    catch (Exception e) {
                        size = 0;
                        value = "?";
                    }
                    Function fn = getFunctionContaining(ins.getAddress());
                    w.println(ins.getAddress() + "\t" + (fn != null ? fn.getName(true) : "?") +
                        "\t" + m + "\t" + t + "\t" + size + "\t" + value + "\t" + b.getName());
                }
            }
        }
    }

    private void writeListing(File f) throws Exception {
        Listing listing = currentProgram.getListing();
        try (PrintWriter w = open(f)) {
            Function cur = null;
            for (Instruction ins : listing.getInstructions(true)) {
                monitor.checkCancelled();
                Function fn = getFunctionContaining(ins.getAddress());
                if (fn != null && fn != cur && fn.getEntryPoint().equals(ins.getAddress())) {
                    w.println();
                    w.println("; ===== " + fn.getName(true) + " @ " + fn.getEntryPoint() + " =====");
                }
                cur = fn;
                w.println(ins.getAddress() + ": " + ins.toString());
            }
        }
    }

    private void writeDecompiled(File f, int timeout, int threads) throws Exception {
        List<Function> fns = new ArrayList<>();
        for (Function fn : currentProgram.getFunctionManager().getFunctions(true)) {
            if (!fn.isThunk()) {
                fns.add(fn);
            }
        }
        String[] out = new String[fns.size()];
        DecompileOptions opts = new DecompileOptions();
        opts.grabFromProgram(currentProgram);
        ConcurrentLinkedQueue<DecompInterface> all = new ConcurrentLinkedQueue<>();
        ThreadLocal<DecompInterface> local = ThreadLocal.withInitial(() -> {
            DecompInterface d = new DecompInterface();
            d.setOptions(opts);
            d.toggleCCode(true);
            d.setSimplificationStyle("decompile");
            d.openProgram(currentProgram);
            all.add(d);
            return d;
        });
        AtomicInteger done = new AtomicInteger();
        AtomicInteger failed = new AtomicInteger();
        ExecutorService pool = Executors.newFixedThreadPool(threads);
        List<Future<?>> futures = new ArrayList<>();
        int total = fns.size();
        println("Decompiling " + total + " functions on " + threads + " threads");
        for (int i = 0; i < total; i++) {
            final int idx = i;
            futures.add(pool.submit(() -> {
                Function fn = fns.get(idx);
                StringBuilder sb = new StringBuilder();
                sb.append("// ===== ").append(fn.getName(true)).append(" @ ")
                        .append(fn.getEntryPoint()).append(" =====\n");
                try {
                    DecompileResults res = local.get().decompileFunction(fn, timeout, TaskMonitor.DUMMY);
                    if (res != null && res.decompileCompleted() && res.getDecompiledFunction() != null) {
                        sb.append(res.getDecompiledFunction().getC()).append('\n');
                    }
                    else {
                        sb.append("// decompile failed: ")
                                .append(res != null ? res.getErrorMessage() : "no result").append("\n\n");
                        failed.incrementAndGet();
                    }
                }
                catch (Throwable t) {
                    sb.append("// decompile exception: ").append(t).append("\n\n");
                    failed.incrementAndGet();
                }
                out[idx] = sb.toString();
                int n = done.incrementAndGet();
                if (n % 2500 == 0) {
                    println("PROGRESS decompiled " + n + "/" + total);
                }
            }));
        }
        pool.shutdown();
        pool.awaitTermination(7, TimeUnit.DAYS);
        for (Future<?> fu : futures) {
            try {
                fu.get();
            }
            catch (Exception e) {
                printerr("worker error: " + e);
            }
        }
        try (PrintWriter w = open(f)) {
            for (String s : out) {
                if (s != null) {
                    w.print(s);
                }
            }
        }
        for (DecompInterface d : all) {
            d.dispose();
        }
        println("Decompiled " + (total - failed.get()) + " functions, " + failed.get() + " failed");
    }

    private static String externalName(Function ext) {
        ExternalLocation loc = ext.getExternalLocation();
        return (loc != null ? loc.getLibraryName() : "?") + "!" + ext.getName();
    }

    private static String escape(String s) {
        return s.replace("\\", "\\\\").replace("\t", "\\t").replace("\r", "\\r").replace("\n", "\\n");
    }
}
