Win32 Chat Panel - Flicker Fix

Bu surumde sohbet paneli tekrar sade Win32 yontemine alindi.
ImGui, Nuklear ve FreeType kullanilmadi.

Yapilan duzeltmeler:
- Chat artik pencere basliginda degil, oyun penceresinin sol altinda Win32 child control olarak gorunur.
- EDIT kontrolu yerine STATIC kontrolu kullanildi; bu sayede caret/scroll repaint kaynakli titreme azaltilir.
- SetWindowTextA artik her frame calismaz; sadece mesaj veya input metni degisirse calisir.
- MoveWindow artik her frame calismaz; sadece pencere boyutu degisirse calisir.
- EM_SETBKGNDCOLOR kullanilmadi; bu nedenle Visual Studio'daki undefined identifier hatasi olusmaz.

Kontroller:
T         : sohbet yazma modunu ac
Enter     : mesaji gonder
Esc       : yazmayi iptal et
Backspace : karakter sil

Not:
Bu yontem Vulkan icine gercek UI cizmek yerine Win32 child control bindirme yontemidir.
Kod sade kalir; ancak profesyonel oyun UI icin ileride Vulkan icinde text renderer kullanmak daha dogrudur.
