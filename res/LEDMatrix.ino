/* -------------------- LED Matrix Text Draw -------------------- */
void showScrolling(const String& s) {
    matrix.beginDraw();
    matrix.textFont(Font_5x7);
    matrix.textScrollSpeed(75);           // ms per pixel
    matrix.beginText(0, 1, 0xFFFFFF);
    matrix.print(" ");
    matrix.print(s);
    matrix.print(" ");
    matrix.endText(SCROLL_LEFT);
    matrix.endDraw();
}