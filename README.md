# Jamm Host (Windows prototipi)

Maqsad: **veb doska bilan yonma-yon qalam kechikishini solishtirish**. Hozircha faqat:
oppoq varaq, bitta qalam, 3 ta rang, 3 xil qalam uchi o'lchami. Sinxronizatsiya, PDF va ovoz yo'q.

Texnologiya: Win32 + `WM_POINTER` (barcha oraliq nuqtalar, bosim) + Direct2D. Bitta fayl: `main.cpp`.

## Olish (GitHub Actions)
1. GitHub → **Actions** → "Build (Windows)" → oxirgi muvaffaqiyatli ishga tushirish.
2. Pastdagi **Artifacts** bo'limidan `JammHost-win-x64` ni yuklab, zip'ni oching → `JammHost.exe`.
3. Qo'lda ishga tushirish: Actions → "Build (Windows)" → **Run workflow**.
(Windows "noma'lum dastur" deb ogohlantirsa: *Qo'shimcha ma'lumot → Baribir ishga tushirish*. Imzolanmagan prototip.)

## Mahalliy yig'ish (Windows, Visual Studio)
"x64 Native Tools Command Prompt" ochib: `build.bat`

## Boshqaruv
| Nima | Qanday |
|---|---|
| Rang | panelda 3 ta doira yoki **1 / 2 / 3** |
| Qalam uchi | panelda 3 ta nuqta yoki **Q / W / E** (2, 5, 10) |
| Bosimni yoqish/o'chirish | **P** |
| Bekor qilish | **Ctrl+Z** |
| Tozalash | **C** |
| Chiqish | **Esc** |
Sichqoncha ham chizadi (qalamsiz sinash uchun).

## O'ng yuqoridagi ko'rsatkichlar
- **turi**: qalam / sichqoncha / barmoq (Windows ko'rayotgan qurilma turi)
- **bosim**: 0..1
- **Hz**: qurilma hisobot chastotasi (planshet sekundiga nechta nuqta beradi)
- **kechikish ms**: *ilova ichidagi* kechikish (qurilma hodisasidan chizish buyrug'i yuborilguncha).
  Drayver va monitor kechikishini **o'z ichiga olmaydi**.

## Solishtirish tartibi (muhim)
1. Bir xil kompyuter, bir xil planshet va qalam.
2. **Veb doska** (Jamm jonli dars, ustoz) va **JammHost.exe** da bir xil chiziqni chizing: tez to'g'ri chiziq, doira, tez zigzag.
3. Telefon kamerasida **sekinlashtirilgan video (240 fps)**: qalam uchi va chiziq uchi orasidagi masofani (kadr soni) solishtiring.
4. XP-Pen drayverida **Windows Ink** yoqiq va o'chiq holatda takrorlang. O'chiq bo'lsa "turi" `sichqoncha` ko'rinishi va bosim yo'qligi mumkin (keyingi bosqichda Wintab zaxirasi kerak).
5. Natijani yozing: qaysi birida qalam uchidan chiziq uchigacha masofa kichikroq, taxminan necha kadr.

## Hozircha yo'q (keyingi bosqich)
Wintab zaxirasi, "wet ink" (InkDesktopHost), bashorat (chiziq uchi), o'chirg'ich, Jamm bilan sinxronizatsiya (Socket.IO), LiveKit ovozi, PDF.
