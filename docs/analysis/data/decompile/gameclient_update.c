// ===== FUN_0064849e @ 0064849e =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void __fastcall FUN_0064849e(int *param_1)

{
  int iVar1;
  char cVar2;
  DWORD DVar3;
  DWORD DVar4;
  int iVar5;
  bool bVar6;
  code *local_c;
  char local_6;
  char local_5;
  
  FUN_0083b471();
  FUN_008392a7();
  FUN_0050eb3c();
  if (DAT_00de4958 != (int *)0x0) {
    (**(code **)(*DAT_00de4958 + 0x6c))();
    FUN_00645750();
  }
  if (DAT_00d9f6fc != '\0') {
    if (DAT_00de7890 != (int *)0x0) {
      *(undefined1 *)(DAT_00de7890 + 0x1b) = 0;
    }
    local_c = FUN_00645b8d;
    FUN_00611c62(&local_c);
    FUN_008000aa();
    local_c = FUN_0064838d;
    FUN_00611c62(&local_c);
    FUN_008000aa();
    local_c = FUN_00645bdf;
    FUN_00611c62(&local_c);
    FUN_008000aa();
    local_c = (code *)&LAB_0064576d;
    FUN_00611c62(&local_c);
    FUN_008000aa();
  }
  DAT_00d9f6fc = 0;
  if (DAT_00de3b54 != (int *)0x0) {
    (**(code **)(*DAT_00de3b54 + 0x28))();
  }
  if (DAT_00de3c24 != (int *)0x0) {
    (**(code **)(*DAT_00de3c24 + 0x28))();
  }
  if (DAT_00de7734 != (int *)0x0) {
    (**(code **)(*DAT_00de7734 + 0x28))();
  }
  if (DAT_00de4efc != (int *)0x0) {
    (**(code **)(*DAT_00de4efc + 0x28))();
  }
  (**(code **)(*DAT_00de4ab0 + 0x28))();
  if (DAT_00de4334 != (int *)0x0) {
    (**(code **)(*DAT_00de4334 + 0x28))();
    (**(code **)(*DAT_00de4334 + 0x3c))();
  }
  (**(code **)(*DAT_00de8b34 + 0x28))();
  (**(code **)(*DAT_00de3670 + 0x28))();
  if (DAT_00de36e0 != (int *)0x0) {
    (**(code **)(*DAT_00de36e0 + 0x28))();
    (**(code **)(*DAT_00de36e0 + 0x40))();
  }
  FUN_00782c56();
  if ((*(char *)(DAT_00de4364 + 0xaf2) == '\0') && (*(char *)(DAT_00de4364 + 0xaf3) == '\0')) {
    (**(code **)(*DAT_00de495c + 0x28))();
    (**(code **)(*DAT_00df06f8 + 0x28))();
    FUN_00532d6f();
    if ((0 < *(int *)(DAT_00de4364 + 0xc78)) &&
       (DVar3 = timeGetTime(), 3000 < DVar3 - _DAT_00de4390)) {
      do {
        DVar4 = timeGetTime();
      } while (DVar4 < *(int *)(DAT_00de4364 + 0xc78) + DVar3);
      _DAT_00de4390 = timeGetTime();
    }
    cVar2 = (**(code **)(*DAT_00de447c + 0xd8))();
    if ((((cVar2 == '\0') || (cVar2 = (**(code **)(*DAT_00de447c + 0x78))(), cVar2 != '\0')) &&
        (cVar2 = FUN_00441e23(), cVar2 == '\0')) &&
       ((cVar2 = FUN_00603418(), cVar2 == '\0' && (cVar2 = FUN_0090f92c(), cVar2 == '\0')))) {
      bVar6 = false;
    }
    else {
      bVar6 = true;
    }
    if (DAT_00de4928 == 0) {
      local_c = (code *)0x0;
    }
    else {
      local_c = *(code **)(*(int *)(DAT_00de4928 + 0x10) + 0x54);
    }
    if ((bVar6) || (local_5 = '\0', DAT_00d9f6f8 == param_1[4])) {
      local_5 = '\x01';
    }
    local_6 = FUN_0063252f();
    if ((local_5 == '\0') && (*(char *)(DAT_00de412c + 0x125) == '\0')) {
      DAT_00d9f6f8 = param_1[4];
      if (local_6 != '\0') {
        (**(code **)(*DAT_00de4bd0 + 0x18))();
      }
      iVar5 = (**(code **)(*param_1 + 0x44))();
      while (iVar5 != 0) {
        iVar1 = *(int *)(iVar5 + 0x104);
        if ((local_6 != '\0') && (*(int *)(iVar5 + 0xfc) != 0)) {
          FUN_0068d8f7();
          FUN_00678f54();
        }
        FUN_00675996();
        iVar5 = iVar1;
      }
      (**(code **)(*DAT_00de8d68 + 0x28))();
      FUN_0063252f();
      FUN_0064594b();
    }
    FUN_0062b385();
    if (*(char *)(DAT_00de412c + 0x125) == '\0') {
      (**(code **)(*(int *)(DAT_00de4ac8 + 4) + 0x28))();
      (**(code **)(*DAT_00de4418 + 0x28))();
    }
    else {
      FUN_0065c1ea();
    }
    if (local_5 == '\0') {
      *(code **)(DAT_00de3744 + 100) = local_c;
    }
    (**(code **)(*DAT_00de4418 + 0x30))();
    (**(code **)(*DAT_00de4518 + 0x28))();
    if (((*(char *)(DAT_00de412c + 0x125) == '\0') || ((char)DAT_00de7890[0x17] != '\0')) &&
       ((**(code **)(*DAT_00de7890 + 0x28))(), *(char *)((int)param_1 + 0xc9) != '\0')) {
      FUN_00645dad();
    }
    (**(code **)(*DAT_00de4830 + 0x28))();
    (**(code **)(*param_1 + 0x90))();
  }
  else {
    (**(code **)(*DAT_00de4418 + 0x30))();
    (**(code **)(*DAT_00de4418 + 0x28))();
  }
  return;
}


