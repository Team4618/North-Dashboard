
button_style menu_button = ButtonStyle(V4(1, 0, 0, 0.5), 40, V2(10, 10), V2(10, 10));
sdfFont font;
texture logoTexture;

void initTheme() {
   font = loadFont("font.fnt", "font.png");
   logoTexture = loadTexture("logo.png", true);
}