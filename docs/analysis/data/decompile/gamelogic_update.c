// ===== FUN_0062e4e8 @ 0062e4e8 =====

void FUN_0062e4e8(void)

{
  undefined1 *puVar1;
  undefined1 uVar2;
  int iVar3;
  char cVar4;
  char *pcVar5;
  undefined4 uVar6;
  undefined4 uVar7;
  int iVar8;
  uint uVar9;
  size_t sVar10;
  int *piVar11;
  int extraout_ECX;
  AsciiString *extraout_ECX_00;
  AsciiString *this;
  char cVar12;
  int unaff_EBP;
  int iVar13;
  bool bVar14;
  
  FUN_00a3cef0();
  FUN_00401174();
  iVar8 = *(int *)(unaff_EBP + 8);
  *(undefined4 *)(unaff_EBP + -4) = 0;
  if (iVar8 == 1) {
    FUN_00604152();
    cVar4 = (**(code **)(*DAT_00de447c + 0xd8))();
    if (((cVar4 == '\0') || (cVar4 = (**(code **)(*DAT_00de447c + 0x78))(), cVar4 != '\0')) &&
       (cVar4 = FUN_0060342f(), cVar4 == '\0')) {
      bVar14 = false;
    }
    else {
      bVar14 = true;
    }
    if (DAT_00de4468 != (int *)0x0) {
      (**(code **)(*DAT_00de4468 + 0xac))();
    }
    cVar4 = FUN_00625130();
    if (cVar4 == '\0') {
LAB_0062e57a:
      if (!bVar14) goto LAB_0062e59a;
    }
    else if (!bVar14) {
      *(int *)(extraout_ECX + 0x40) = *(int *)(extraout_ECX + 0x40) + 1;
      goto LAB_0062e57a;
    }
    cVar4 = (**(code **)(*DAT_00de639c + 0x44))(0x1d);
    if (cVar4 == '\0') {
      *(undefined1 *)(DAT_00de4388 + 200) = 0;
      goto LAB_0062ee54;
    }
    FUN_006034fb();
  }
LAB_0062e59a:
  FUN_005ff9d3(*(undefined4 *)(extraout_ECX + 0x164),*(undefined4 *)(extraout_ECX + 0x168));
  if ((*(char *)(extraout_ECX + 0x125) != '\0') && (cVar4 = FUN_00441b7c(), cVar4 == '\0')) {
    if (iVar8 == 1) {
      FUN_00625804();
      *(undefined1 *)(DAT_00de4388 + 200) = 1;
    }
    *(int *)(extraout_ECX + 0x17c) = iVar8;
    goto LAB_0062ee54;
  }
  puVar1 = (undefined1 *)(extraout_ECX + 0x70);
  *(int *)(extraout_ECX + 0x17c) = iVar8;
  uVar2 = *puVar1;
  *(bool *)(unaff_EBP + -0xd) = iVar8 == 1;
  *(undefined1 **)(unaff_EBP + -0x28) = puVar1;
  *(undefined ***)(unaff_EBP + -0x38) = &PTR_LAB_00bdb2b4;
  *(undefined1 **)(unaff_EBP + -0x30) = puVar1;
  *(undefined1 *)(unaff_EBP + -0x34) = uVar2;
  *puVar1 = 1;
  bVar14 = DAT_00de4148 == '\0';
  *(undefined1 *)(unaff_EBP + -4) = 1;
  if (bVar14) {
LAB_0062e63a:
    if (((*(char *)(unaff_EBP + -0xd) != '\0') && (*(int *)(extraout_ECX + 0x40) == 2)) &&
       ((cVar4 = FUN_00610b62(), cVar4 != '\0' && (cVar4 = FUN_006253bf(), cVar4 != '\0')))) {
      for (iVar8 = *(int *)(DAT_00de412c + 0xac); iVar8 != 0; iVar8 = *(int *)(iVar8 + 0x8c)) {
        if ((*(int *)(iVar8 + 0x260) != 0) && (cVar4 = FUN_00664485(), cVar4 != '\0')) {
          FUN_0066276b();
        }
      }
    }
  }
  else if (*(char *)(unaff_EBP + -0xd) != '\0') {
    FUN_0062c159();
    goto LAB_0062e63a;
  }
  FUN_006f2364();
  FUN_00440809();
  if (*(char *)(unaff_EBP + -0xd) != '\0') {
    (**(code **)(*DAT_00de3bac + 0x28))();
    (**(code **)(*DAT_00de7804 + 0x28))();
    (**(code **)(*(int *)(DAT_00de4690 + 4) + 0x28))();
    if (DAT_00de897c != (int *)0x0) {
      (**(code **)(*DAT_00de897c + 0x28))();
    }
  }
  cVar4 = FUN_00441b60();
  if (((cVar4 != '\0') && (DAT_00de7cd8 != (int *)0x0)) && (*(char *)(unaff_EBP + -0xd) != '\0')) {
    cVar12 = '\0';
    cVar4 = FUN_0077d627();
    this = extraout_ECX_00;
    if (cVar4 != '\0') {
      this = *(AsciiString **)(DAT_00de892c + 0xc);
      cVar12 = '\x01' - (*(uint *)(extraout_ECX + 0x40) % (uint)this != 0);
      if (*(int *)(extraout_ECX + 0x110) == 2) {
        cVar12 = '\0';
      }
    }
    if (DAT_00da62ec == (AsciiString *)0xffffffff) {
LAB_0062e75d:
      if (cVar12 != '\0') goto LAB_0062e765;
    }
    else {
      this = *(AsciiString **)(extraout_ECX + 0x40);
      if ((this < DAT_00da62ec + (-2 - *(int *)(DAT_00de4364 + 0xc18))) || (DAT_00da62ec < this)) {
        cVar12 = '\0';
        goto LAB_0062e75d;
      }
LAB_0062e765:
      uVar6 = *(undefined4 *)(*(int *)(DAT_00de4928 + 0x10) + 0x54);
      *(undefined4 *)(unaff_EBP + -0x14) = 0;
      bVar14 = DAT_00de87c6 == '\0';
      *(undefined4 *)(unaff_EBP + -0x1c) = uVar6;
      if (bVar14) {
        DAT_00de87c7 = '\x01';
        uVar6 = FUN_00625886(0);
        DAT_00de87c7 = '\0';
      }
      else {
        *(undefined4 *)(unaff_EBP + -0x18) = 0;
        uVar6 = *(undefined4 *)(extraout_ECX + 0x40);
        *(undefined1 *)(unaff_EBP + -4) = 2;
        AsciiString::format(this,(char *)(unaff_EBP + -0x18),&DAT_00bd4194,uVar6);
        if (*(int *)(unaff_EBP + -0x18) == 0) {
          pcVar5 = &`public:_char_const*___thiscall_StringBase<char>::str(void)const_'::__l2::
                    TheNullChr;
        }
        else {
          pcVar5 = (char *)(*(int *)(unaff_EBP + -0x18) + 8);
        }
        uVar6 = FUN_00a1611e(pcVar5);
        *(undefined4 *)(unaff_EBP + -0x14) = uVar6;
        FUN_00ad9ab0(&DAT_00bfdc38);
        uVar6 = FUN_00625886(*(undefined4 *)(unaff_EBP + -0x14));
        FUN_00ad98f0(&DAT_00bfdc38);
        *(undefined1 *)(unaff_EBP + -4) = 1;
        StringBase<char>::releaseBuffer((StringBase<char> *)(unaff_EBP + -0x18));
      }
      uVar7 = (**(code **)(*DAT_00de6398 + 0x48))(0x44a);
      FUN_007111e5(uVar6);
      FUN_007111b5(*(undefined4 *)(extraout_ECX + 0x38));
      FUN_007111b5(*(undefined4 *)(extraout_ECX + 0x40));
      iVar8 = FUN_007b0f25();
      FUN_00711104(CONCAT31((int3)((uint)-(iVar8 + -1) >> 8),'\x01' - (iVar8 + -1 != 0)));
      if ((DAT_00de87c6 == '\0') && (DAT_00de87c7 == '\0')) {
        FUN_00711104(0);
      }
      else {
        FUN_0062a413(uVar6,*(undefined4 *)(unaff_EBP + -0x1c),*(undefined4 *)(extraout_ECX + 0x40),
                     uVar7,0,*(undefined4 *)(unaff_EBP + -0x14));
      }
    }
    if (DAT_00de4a30 != 0) {
      FUN_006cf860();
    }
  }
  if (DAT_00de8a98 == 0) {
LAB_0062e89e:
    if (*(char *)(unaff_EBP + -0xd) != '\0') {
      (**(code **)(*DAT_00de7cd8 + 0x28))();
      (**(code **)(*DAT_00de46a8 + 0x28))();
      (**(code **)(*DAT_00de772c + 0x28))();
      FUN_00820ef0();
      FUN_0081be85();
      for (iVar8 = DAT_00de639c[3]; iVar8 != 0; iVar8 = *(int *)(iVar8 + 4)) {
        FUN_00779a3d(iVar8,0);
      }
      (**(code **)(*DAT_00de639c + 0x24))();
      for (iVar8 = *(int *)(extraout_ECX + 0xac); iVar8 != 0; iVar8 = *(int *)(iVar8 + 0x8c)) {
        iVar13 = FUN_0070e013();
        if (iVar13 != 0) {
          uVar6 = 0;
          FUN_0070e013(0);
          FUN_00674b1f(uVar6);
        }
      }
    }
  }
  else if (*(char *)(unaff_EBP + -0xd) != '\0') {
    FUN_0081a0a7();
    goto LAB_0062e89e;
  }
  iVar8 = *(int *)(unaff_EBP + 8);
  bVar14 = iVar8 == 2;
  if (bVar14) {
    (**(code **)(*DAT_00de4354 + 0x28))();
    (**(code **)(*DAT_00de4360 + 0x28))();
    iVar8 = *(int *)(extraout_ECX + 0xac);
    if (iVar8 != 0) {
      do {
        if (*(int *)(iVar8 + 0x188) != *(int *)(extraout_ECX + 0x40)) {
          FUN_006260e1(*(int *)(extraout_ECX + 0x40));
        }
        iVar8 = *(int *)(iVar8 + 0x8c);
      } while (iVar8 != 0);
      iVar8 = *(int *)(unaff_EBP + 8);
      bVar14 = iVar8 == 2;
      goto LAB_0062e982;
    }
  }
  else {
LAB_0062e982:
    if (!bVar14 && 1 < iVar8) {
      iVar8 = *(int *)(unaff_EBP + 8);
      iVar13 = 0;
      *(undefined4 *)(unaff_EBP + -0x20) = 0;
      if (2 < iVar8) {
        if (iVar8 < 5) {
          *(undefined4 *)(unaff_EBP + -0x20) = 1;
        }
        else if (iVar8 == 5) {
          iVar13 = 1;
          *(undefined4 *)(unaff_EBP + -0x20) = 3;
        }
        else if (iVar8 == 6) {
          *(undefined4 *)(unaff_EBP + -0x20) = 4;
          iVar13 = 3;
        }
      }
      *(int *)(unaff_EBP + -0x18) = iVar13;
      if (iVar13 < *(int *)(unaff_EBP + -0x20)) {
        piVar11 = (int *)(iVar13 * 0xc + 200 + extraout_ECX);
        do {
          if (*(int *)(unaff_EBP + 8) == 4) {
            iVar8 = ((uint)(piVar11[1] - *piVar11 >> 2) >> 1) - 1;
          }
          else {
            iVar8 = -1;
          }
          if (iVar8 + 1U < (uint)(piVar11[1] - *piVar11 >> 2)) {
            *(uint *)(unaff_EBP + -0x24) = iVar8 + 1U;
            do {
              *(int *)(unaff_EBP + -0x24) = *(int *)(unaff_EBP + -0x24) + 1;
              *(uint *)(unaff_EBP + -0x1c) = iVar8 + 1U;
              if ((*(int *)(unaff_EBP + 8) == 3) &&
                 (iVar8 + 1U == (uint)(piVar11[1] - *piVar11 >> 2) >> 1)) goto LAB_0062eb51;
              iVar8 = *(int *)(*piVar11 + *(int *)(unaff_EBP + -0x1c) * 4);
              if ((iVar8 != 0) && (*(uint *)(iVar8 + 0x14) <= *(uint *)(DAT_00de412c + 0x40))) {
                iVar13 = *(int *)(iVar8 + 8);
                *(undefined4 *)(unaff_EBP + -0x14) = 1;
                *(int *)(unaff_EBP + -0x2c) = iVar13 + 0x1c8;
                cVar4 = FUN_009325b4();
                if (cVar4 == '\0') {
LAB_0062ea7c:
                  *(int *)(extraout_ECX + 0x104) = iVar8;
                  if ((*(byte *)(*(int *)(iVar8 + 8) + 0x94) & 1) == 0) {
                    iVar13 = (*(code *)**(undefined4 **)(iVar8 + 0x10))();
                    *(int *)(unaff_EBP + -0x14) = iVar13;
                    if (iVar13 < 1) {
                      *(undefined4 *)(unaff_EBP + -0x14) = 1;
                    }
                  }
                  else {
                    *(undefined4 *)(unaff_EBP + -0x14) = 0x3fffffff;
                  }
                  *(undefined4 *)(extraout_ECX + 0x104) = 0;
                }
                else {
                  (**(code **)(*(int *)(iVar8 + 0x10) + 4))(unaff_EBP + -0x3c);
                  cVar4 = FUN_006252e4(*(undefined4 *)(unaff_EBP + -0x2c));
                  if (cVar4 != '\0') goto LAB_0062ea7c;
                }
                uVar9 = *(int *)(DAT_00de412c + 0x40) + *(int *)(unaff_EBP + -0x14);
                if (0x3fffffff < uVar9) {
                  uVar9 = 0x3fffffff;
                }
                *(uint *)(iVar8 + 0x14) = uVar9;
              }
              if ((uint)(piVar11[1] - *piVar11 >> 2) <= *(uint *)(unaff_EBP + -0x24)) break;
              iVar8 = *(int *)(unaff_EBP + -0x1c);
            } while( true );
          }
          if (3 < *(int *)(unaff_EBP + 8)) {
            uVar9 = piVar11[1] - *piVar11 >> 2;
            while (uVar9 != 0) {
              iVar8 = *piVar11;
              uVar9 = uVar9 - 1;
              iVar13 = *(int *)(iVar8 + uVar9 * 4);
              *(int *)(unaff_EBP + -0x1c) = iVar13;
              if ((iVar13 != 0) && (0x3ffffffe < *(uint *)(iVar13 + 0x14))) {
                if (uVar9 < (piVar11[1] - iVar8 >> 2) - 1U) {
                  *(undefined4 *)(*piVar11 + uVar9 * 4) = *(undefined4 *)(piVar11[1] + -4);
                  iVar8 = *(int *)(*piVar11 + uVar9 * 4);
                  *(undefined4 *)(iVar8 + 0x1c) = *(undefined4 *)(unaff_EBP + -0x18);
                  *(uint *)(iVar8 + 0x18) = uVar9;
                }
                piVar11[1] = piVar11[1] + -4;
                iVar8 = *(int *)(extraout_ECX + 0xfc);
                iVar3 = *(int *)(extraout_ECX + 0xf8);
                *(undefined4 *)(iVar13 + 0x1c) = 0xffffffff;
                *(int *)(iVar13 + 0x18) = iVar8 - iVar3 >> 2;
                FUN_0090be00(unaff_EBP + -0x1c);
              }
            }
          }
LAB_0062eb51:
          *(int *)(unaff_EBP + -0x18) = *(int *)(unaff_EBP + -0x18) + 1;
          piVar11 = piVar11 + 3;
        } while (*(int *)(unaff_EBP + -0x18) < *(int *)(unaff_EBP + -0x20));
      }
    }
    if (*(int *)(unaff_EBP + 8) == 5) {
      (**(code **)(*DAT_00de4b40 + 0x28))();
    }
  }
  FUN_00629da6();
  if (*(int *)(unaff_EBP + 8) == 5) {
    (**(code **)(*DAT_00de4358 + 0x28))();
    (**(code **)(*DAT_00de435c + 0x28))();
    (**(code **)(*DAT_00de8200 + 0x28))();
    (**(code **)(*DAT_00de3be8 + 0x28))();
    FUN_0062a2c9();
    (**(code **)(*DAT_00de4a1c + 0x28))();
    (**(code **)(*DAT_00de369c + 0x28))();
    (**(code **)(*DAT_00de89ac + 0x28))();
    (**(code **)(*DAT_00de8ac0 + 0x28))();
    FUN_0080f4d3();
    (**(code **)(*DAT_00de4938 + 0x28))();
    (**(code **)(*DAT_00de7924 + 0x28))();
    (**(code **)(*DAT_00de8304 + 0x28))();
  }
  cVar4 = FUN_00441b60();
  if (((cVar4 != '\0') && (*(int *)(extraout_ECX + 0x40) == 0x400)) &&
     (cVar4 = FUN_0063f7d8(), cVar4 == '\0')) {
    (**(code **)(*DAT_00de6398 + 0x48))(0x448);
    FUN_00711104(0);
  }
  if (*(char *)(unaff_EBP + -0xd) != '\0') {
    for (iVar8 = *(int *)(extraout_ECX + 0xac); iVar8 != 0; iVar8 = *(int *)(iVar8 + 0x8c)) {
      cVar4 = FUN_009325b4();
      if (cVar4 != '\0') {
        FUN_00690a42();
      }
      cVar4 = FUN_0044ddec(0x4a);
      if (cVar4 != '\0') {
        FUN_00690ab9();
      }
      cVar4 = FUN_0044ddec(4);
      if (cVar4 != '\0') {
        FUN_00690ae5();
      }
      FUN_00697eb6();
    }
    if ((*(char *)(extraout_ECX + 0xa8) == '\0') && (*(char *)(extraout_ECX + 0x44) != '\0')) {
      FUN_006251a3(*(int *)(extraout_ECX + 0x40) * 10);
      if (*(char *)(DAT_00de4364 + 0x123d) != '\0') {
        DAT_00def550 = *(undefined4 *)(extraout_ECX + 0x40);
      }
      if ((*(char *)(DAT_00de4364 + 0xd45) != '\0') && (0 < (int)*(uint *)(DAT_00de4364 + 0xd48))) {
        if (*(uint *)(DAT_00de4364 + 0xd48) < *(uint *)(extraout_ECX + 0x40)) {
          (**(code **)(*DAT_00de4324 + 0x50))(1);
        }
        else {
          if (*(int *)(DAT_00de4364 + 0xc) == 0) {
            pcVar5 = &`public:_char_const*___thiscall_StringBase<char>::str(void)const_'::__l2::
                      TheNullChr;
          }
          else {
            pcVar5 = (char *)(*(int *)(DAT_00de4364 + 0xc) + 8);
          }
          _mbscpy((uchar *)(unaff_EBP + -0x180),(uchar *)pcVar5);
          sVar10 = strlen((char *)(unaff_EBP + -0x180));
          if (3 < (int)sVar10) {
            for (pcVar5 = (char *)(unaff_EBP + -0x184 + sVar10);
                (((char *)(unaff_EBP + -0x180) < pcVar5 && (*pcVar5 != '\\')) && (*pcVar5 != '/'));
                pcVar5 = pcVar5 + -1) {
            }
            *pcVar5 = '\0';
            _mbscat((uchar *)(unaff_EBP + -0x180),(uchar *)s__scenecapture_dat_00bfdc24);
            if (*(int *)(extraout_ECX + 0x40) == 1) {
              _unlink((char *)(unaff_EBP + -0x180));
            }
            piVar11 = (int *)FUN_00a149a2(unaff_EBP + -0x180,0x4b,0);
            if (piVar11 != (int *)0x0) {
              uVar6 = (**(code **)(*piVar11 + 0x14))(0,2);
              iVar8 = *piVar11;
              *(undefined4 *)(unaff_EBP + 8) = 0;
              (**(code **)(iVar8 + 0x10))(unaff_EBP + 8,4);
              FUN_00a20edc();
              *(undefined1 *)(unaff_EBP + -4) = 3;
              FUN_00a20dd0(piVar11,0,0);
              FUN_00645655(unaff_EBP + -0x7c);
              FUN_00a205ad();
              uVar7 = (**(code **)(*piVar11 + 0x14))(0,1);
              *(undefined4 *)(unaff_EBP + 8) = uVar7;
              (**(code **)(*piVar11 + 0x14))(uVar6,0);
              (**(code **)(*piVar11 + 0x10))(unaff_EBP + 8,4);
              (**(code **)(*piVar11 + 8))();
              *(undefined1 *)(unaff_EBP + -4) = 1;
              FUN_00a20d79();
            }
          }
        }
      }
    }
    *(undefined1 *)(DAT_00de4388 + 200) = 1;
  }
  **(undefined1 **)(unaff_EBP + -0x28) = *(undefined1 *)(unaff_EBP + -0x34);
LAB_0062ee54:
  if (DAT_00de412c != 0) {
    *(int *)(DAT_00de412c + 0x1b4) = *(int *)(DAT_00de412c + 0x1b4) + -1;
  }
  ExceptionList = *(void **)(unaff_EBP + -0xc);
  return;
}


