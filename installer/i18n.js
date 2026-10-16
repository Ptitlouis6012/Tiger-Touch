// Translations for the web installer.
//
// The eleven languages the device itself speaks (firmware/main/i18n_table.h),
// in the same order. `pt` and `pt-pt` are kept apart on purpose: Brazilian and
// European Portuguese differ in exactly the words this page uses.
//
// Unlike the device's strings, these carry accents: this is HTML drawn by a
// browser, not LVGL drawing from a font subset.
//
// The window that opens after the button belongs to ESP Web Tools and is in
// English only. The labels it shows (`Install Tiger Touch`, `Erase device`,
// `Next`) are therefore quoted untranslated, so that someone reading the page in
// Polish can find them on the screen. Keep them that way.
//
// Keys ending in `_html` may contain <b> and nothing else; everything else is
// inserted as text. scripts/check-installer-i18n.py enforces the shape: the same
// keys in every language, none empty, and the button named in step 2 spelled
// as the button itself.

window.I18N = {

en: {
  lang_name: "English",
  lang_label: "Choose a language",
  title: "Install Tiger Touch",
  tagline: "Install Tiger Touch on a BIGTREETECH Panda Touch or K-Touch, from this page.",
  fw: "Firmware",
  cta_btn: "Install",
  n1: "Plug the screen into this computer with its USB-C cable.",
  n2_html: "Press <b>Install</b> and pick the serial port (a \"USB-SERIAL CH340 / USB JTAG/serial debug unit\" entry).",
  n3_html: "In the window that opens, which is in English, choose <b>Install Tiger Touch</b>. Coming from the BIGTREETECH firmware, tick <b>Erase device</b>; updating a Tiger Touch, leave it unticked. Then press <b>Next</b>.",
  n4: "Wait for it to finish. The board restarts by itself.",
  unsup_html: "This browser cannot talk to a serial port. Use <b>Chrome</b> or <b>Edge</b> on a computer; Safari, Firefox and phones do not support WebSerial.",
  keep_html: "Your saved Wi-Fi and account are kept. Ticking <b>Erase device</b> wipes them and gives a first start, in English.",
  driver: "No driver to hunt for on a recent Windows; on an older one or a Mac, install the CH340 driver if no port appears.",
},

fr: {
  lang_name: "Français",
  lang_label: "Choisir une langue",
  title: "Installer Tiger Touch",
  tagline: "Installez Tiger Touch sur un BIGTREETECH Panda Touch ou K-Touch, depuis cette page.",
  fw: "Micrologiciel",
  cta_btn: "Installer",
  n1: "Branchez l'écran à cet ordinateur avec son câble USB-C.",
  n2_html: "Appuyez sur <b>Installer</b> et choisissez le port série (une entrée « USB-SERIAL CH340 / USB JTAG/serial debug unit »).",
  n3_html: "Dans la fenêtre qui s'ouvre, en anglais, choisissez <b>Install Tiger Touch</b>. Si l'écran a encore le firmware BIGTREETECH, cochez <b>Erase device</b> ; pour mettre à jour un Tiger Touch, laissez-le décoché. Puis appuyez sur <b>Next</b>.",
  n4: "Attendez la fin de l'opération. La carte redémarre toute seule.",
  unsup_html: "Ce navigateur ne peut pas communiquer avec un port série. Utilisez <b>Chrome</b> ou <b>Edge</b> sur un ordinateur ; Safari, Firefox et les téléphones ne prennent pas en charge WebSerial.",
  keep_html: "Votre Wi-Fi et votre compte enregistrés sont conservés. Cocher <b>Erase device</b> les efface et donne un premier démarrage, en anglais.",
  driver: "Aucun pilote à chercher sous un Windows récent ; sous un plus ancien ou sur Mac, installez le pilote CH340 si aucun port n'apparaît.",
},

de: {
  lang_name: "Deutsch",
  lang_label: "Sprache wählen",
  title: "Tiger Touch installieren",
  tagline: "Installieren Sie Tiger Touch auf einem BIGTREETECH Panda Touch oder K-Touch, direkt von dieser Seite aus.",
  fw: "Firmware",
  cta_btn: "Installieren",
  n1: "Verbinden Sie den Bildschirm mit seinem USB-C-Kabel mit diesem Computer.",
  n2_html: "Klicken Sie auf <b>Installieren</b> und wählen Sie den seriellen Port (ein Eintrag „USB-SERIAL CH340 / USB JTAG/serial debug unit“).",
  n3_html: "Wählen Sie im Fenster, das sich öffnet (es ist auf Englisch), <b>Install Tiger Touch</b>. Läuft noch die BIGTREETECH-Firmware, aktivieren Sie <b>Erase device</b>; beim Aktualisieren eines Tiger Touch lassen Sie es deaktiviert. Klicken Sie dann auf <b>Next</b>.",
  n4: "Warten Sie, bis der Vorgang abgeschlossen ist. Das Board startet von selbst neu.",
  unsup_html: "Dieser Browser kann nicht mit einem seriellen Port kommunizieren. Verwenden Sie <b>Chrome</b> oder <b>Edge</b> auf einem Computer; Safari, Firefox und Smartphones unterstützen WebSerial nicht.",
  keep_html: "Ihr gespeichertes WLAN und Ihr Konto bleiben erhalten. Wenn Sie <b>Erase device</b> aktivieren, werden sie gelöscht und das Gerät startet wie beim ersten Mal, auf Englisch.",
  driver: "Unter einem aktuellen Windows ist kein Treiber nötig; unter einem älteren oder auf einem Mac installieren Sie den CH340-Treiber, falls kein Port erscheint.",
},

es: {
  lang_name: "Español",
  lang_label: "Elegir idioma",
  title: "Instalar Tiger Touch",
  tagline: "Instala Tiger Touch en un BIGTREETECH Panda Touch o K-Touch desde esta página.",
  fw: "Firmware",
  cta_btn: "Instalar",
  n1: "Conecta la pantalla a este ordenador con su cable USB-C.",
  n2_html: "Pulsa <b>Instalar</b> y elige el puerto serie (una entrada «USB-SERIAL CH340 / USB JTAG/serial debug unit»).",
  n3_html: "En la ventana que se abre, que está en inglés, elige <b>Install Tiger Touch</b>. Si la pantalla aún tiene el firmware de BIGTREETECH, marca <b>Erase device</b>; para actualizar un Tiger Touch, déjalo sin marcar. Luego pulsa <b>Next</b>.",
  n4: "Espera a que termine. La placa se reinicia sola.",
  unsup_html: "Este navegador no puede comunicarse con un puerto serie. Usa <b>Chrome</b> o <b>Edge</b> en un ordenador; Safari, Firefox y los móviles no admiten WebSerial.",
  keep_html: "Tu Wi-Fi y tu cuenta guardados se conservan. Si marcas <b>Erase device</b>, se borran y se obtiene un primer arranque, en inglés.",
  driver: "En un Windows reciente no hay que buscar ningún controlador; en uno antiguo o en un Mac, instala el controlador CH340 si no aparece ningún puerto.",
},

it: {
  lang_name: "Italiano",
  lang_label: "Scegli la lingua",
  title: "Installa Tiger Touch",
  tagline: "Installa Tiger Touch su un BIGTREETECH Panda Touch o K-Touch direttamente da questa pagina.",
  fw: "Firmware",
  cta_btn: "Installa",
  n1: "Collega lo schermo a questo computer con il suo cavo USB-C.",
  n2_html: "Premi <b>Installa</b> e scegli la porta seriale (una voce «USB-SERIAL CH340 / USB JTAG/serial debug unit»).",
  n3_html: "Nella finestra che si apre, in inglese, scegli <b>Install Tiger Touch</b>. Se lo schermo ha ancora il firmware BIGTREETECH, seleziona <b>Erase device</b>; per aggiornare un Tiger Touch, lascialo deselezionato. Poi premi <b>Next</b>.",
  n4: "Attendi la fine dell'operazione. La scheda si riavvia da sola.",
  unsup_html: "Questo browser non può comunicare con una porta seriale. Usa <b>Chrome</b> o <b>Edge</b> su un computer; Safari, Firefox e i telefoni non supportano WebSerial.",
  keep_html: "Il Wi-Fi e l'account salvati vengono conservati. Selezionando <b>Erase device</b> vengono cancellati e si ottiene un primo avvio, in inglese.",
  driver: "Su un Windows recente non serve alcun driver; su uno più vecchio o su un Mac, installa il driver CH340 se non compare nessuna porta.",
},

zh: {
  lang_name: "中文",
  lang_label: "选择语言",
  title: "安装 Tiger Touch",
  tagline: "在此页面上为 BIGTREETECH Panda Touch 或 K-Touch 安装 Tiger Touch。",
  fw: "固件",
  cta_btn: "安装",
  n1: "用屏幕自带的 USB-C 线将它连接到这台电脑。",
  n2_html: "点击<b>安装</b>，然后选择串口（名为“USB-SERIAL CH340 / USB JTAG/serial debug unit”的条目）。",
  n3_html: "在随后打开的窗口（英文界面）中选择 <b>Install Tiger Touch</b>。如果屏幕上仍是 BIGTREETECH 固件，请勾选 <b>Erase device</b>；如果是更新 Tiger Touch，则不要勾选。然后点击 <b>Next</b>。",
  n4: "等待安装完成。开发板会自动重启。",
  unsup_html: "此浏览器无法与串口通信。请在电脑上使用 <b>Chrome</b> 或 <b>Edge</b>；Safari、Firefox 和手机浏览器不支持 WebSerial。",
  keep_html: "已保存的 Wi-Fi 和账号会保留。勾选 <b>Erase device</b> 会将其清除，设备将按首次启动处理，使用英文界面。",
  driver: "较新的 Windows 无需另找驱动；较旧的 Windows 或 Mac 上，如果没有出现端口，请安装 CH340 驱动。",
},

pt: {
  lang_name: "Português (BR)",
  lang_label: "Escolher idioma",
  title: "Instalar Tiger Touch",
  tagline: "Instale o Tiger Touch em um BIGTREETECH Panda Touch ou K-Touch, direto desta página.",
  fw: "Firmware",
  cta_btn: "Instalar",
  n1: "Conecte a tela a este computador com o cabo USB-C dela.",
  n2_html: "Clique em <b>Instalar</b> e escolha a porta serial (uma entrada «USB-SERIAL CH340 / USB JTAG/serial debug unit»).",
  n3_html: "Na janela que abre, que está em inglês, escolha <b>Install Tiger Touch</b>. Se a tela ainda tem o firmware da BIGTREETECH, marque <b>Erase device</b>; para atualizar um Tiger Touch, deixe desmarcado. Depois clique em <b>Next</b>.",
  n4: "Aguarde até terminar. A placa reinicia sozinha.",
  unsup_html: "Este navegador não consegue se comunicar com uma porta serial. Use o <b>Chrome</b> ou o <b>Edge</b> em um computador; Safari, Firefox e celulares não são compatíveis com WebSerial.",
  keep_html: "O Wi-Fi e a conta salvos são mantidos. Marcar <b>Erase device</b> apaga tudo isso e dá uma primeira inicialização, em inglês.",
  driver: "Em um Windows recente não é preciso procurar driver; em um mais antigo ou em um Mac, instale o driver CH340 se nenhuma porta aparecer.",
},

"pt-pt": {
  lang_name: "Português (PT)",
  lang_label: "Escolher idioma",
  title: "Instalar Tiger Touch",
  tagline: "Instale o Tiger Touch num BIGTREETECH Panda Touch ou K-Touch, diretamente a partir desta página.",
  fw: "Firmware",
  cta_btn: "Instalar",
  n1: "Ligue o ecrã a este computador com o respetivo cabo USB-C.",
  n2_html: "Clique em <b>Instalar</b> e escolha a porta série (uma entrada «USB-SERIAL CH340 / USB JTAG/serial debug unit»).",
  n3_html: "Na janela que abre, que está em inglês, escolha <b>Install Tiger Touch</b>. Se o ecrã ainda tem o firmware da BIGTREETECH, marque <b>Erase device</b>; para atualizar um Tiger Touch, deixe sem marcar. Depois clique em <b>Next</b>.",
  n4: "Aguarde que termine. A placa reinicia sozinha.",
  unsup_html: "Este navegador não consegue comunicar com uma porta série. Use o <b>Chrome</b> ou o <b>Edge</b> num computador; o Safari, o Firefox e os telemóveis não suportam WebSerial.",
  keep_html: "O Wi-Fi e a conta guardados são mantidos. Marcar <b>Erase device</b> apaga-os e dá um primeiro arranque, em inglês.",
  driver: "Num Windows recente não é preciso procurar nenhum controlador; num mais antigo ou num Mac, instale o controlador CH340 se não aparecer nenhuma porta.",
},

pl: {
  lang_name: "Polski",
  lang_label: "Wybierz język",
  title: "Zainstaluj Tiger Touch",
  tagline: "Zainstaluj Tiger Touch na BIGTREETECH Panda Touch lub K-Touch bezpośrednio z tej strony.",
  fw: "Firmware",
  cta_btn: "Zainstaluj",
  n1: "Podłącz ekran do tego komputera kablem USB-C.",
  n2_html: "Naciśnij <b>Zainstaluj</b> i wybierz port szeregowy (pozycja „USB-SERIAL CH340 / USB JTAG/serial debug unit”).",
  n3_html: "W oknie, które się otworzy (jest po angielsku), wybierz <b>Install Tiger Touch</b>. Jeśli na ekranie jest jeszcze firmware BIGTREETECH, zaznacz <b>Erase device</b>; przy aktualizacji Tiger Touch pozostaw je niezaznaczone. Następnie naciśnij <b>Next</b>.",
  n4: "Poczekaj, aż skończy. Płytka uruchomi się ponownie sama.",
  unsup_html: "Ta przeglądarka nie może komunikować się z portem szeregowym. Użyj <b>Chrome</b> lub <b>Edge</b> na komputerze; Safari, Firefox i telefony nie obsługują WebSerial.",
  keep_html: "Zapisane Wi-Fi i konto zostają zachowane. Zaznaczenie <b>Erase device</b> usuwa je i daje pierwsze uruchomienie, po angielsku.",
  driver: "W nowszym Windows nie trzeba szukać sterownika; w starszym lub na Macu zainstaluj sterownik CH340, jeśli port się nie pojawi.",
},

ru: {
  lang_name: "Русский",
  lang_label: "Выберите язык",
  title: "Установка Tiger Touch",
  tagline: "Установите Tiger Touch на BIGTREETECH Panda Touch или K-Touch прямо с этой страницы.",
  fw: "Прошивка",
  cta_btn: "Установить",
  n1: "Подключите экран к этому компьютеру кабелем USB-C.",
  n2_html: "Нажмите <b>Установить</b> и выберите последовательный порт (пункт «USB-SERIAL CH340 / USB JTAG/serial debug unit»).",
  n3_html: "В открывшемся окне (оно на английском) выберите <b>Install Tiger Touch</b>. Если на экране ещё прошивка BIGTREETECH, поставьте галочку <b>Erase device</b>; при обновлении Tiger Touch оставьте её пустой. Затем нажмите <b>Next</b>.",
  n4: "Дождитесь окончания. Плата перезагрузится сама.",
  unsup_html: "Этот браузер не может работать с последовательным портом. Используйте <b>Chrome</b> или <b>Edge</b> на компьютере; Safari, Firefox и телефоны не поддерживают WebSerial.",
  keep_html: "Сохранённые Wi-Fi и аккаунт остаются. Если поставить галочку <b>Erase device</b>, они удаляются, и устройство запустится как в первый раз, на английском.",
  driver: "В современной Windows драйвер искать не нужно; в более старой или на Mac установите драйвер CH340, если порт не появился.",
},

nl: {
  lang_name: "Nederlands",
  lang_label: "Kies een taal",
  title: "Tiger Touch installeren",
  tagline: "Installeer Tiger Touch op een BIGTREETECH Panda Touch of K-Touch, rechtstreeks vanaf deze pagina.",
  fw: "Firmware",
  cta_btn: "Installeren",
  n1: "Sluit het scherm met zijn USB-C-kabel aan op deze computer.",
  n2_html: "Klik op <b>Installeren</b> en kies de seriële poort (een item “USB-SERIAL CH340 / USB JTAG/serial debug unit”).",
  n3_html: "Kies in het venster dat opent (het is in het Engels) <b>Install Tiger Touch</b>. Staat er nog BIGTREETECH-firmware op het scherm, vink dan <b>Erase device</b> aan; bij het bijwerken van een Tiger Touch laat u het uitgevinkt. Klik daarna op <b>Next</b>.",
  n4: "Wacht tot het klaar is. Het bord start vanzelf opnieuw op.",
  unsup_html: "Deze browser kan niet met een seriële poort communiceren. Gebruik <b>Chrome</b> of <b>Edge</b> op een computer; Safari, Firefox en telefoons ondersteunen WebSerial niet.",
  keep_html: "De opgeslagen wifi en het account blijven behouden. Met <b>Erase device</b> aangevinkt worden ze gewist en volgt een eerste start, in het Engels.",
  driver: "Op een recente Windows hoeft er geen stuurprogramma gezocht te worden; installeer op een oudere versie of op een Mac het CH340-stuurprogramma als er geen poort verschijnt.",
},

};
