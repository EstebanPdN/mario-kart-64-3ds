"""Render the complete HD menu alphabet from transparent generated textures."""
import json
from pathlib import Path
from PySide6.QtCore import QRectF
from PySide6.QtGui import QImage


class GameFont:
    def __init__(self):
        root = Path(__file__).with_name('assets')
        data = json.loads((root / 'font-hd.json').read_text())
        atlas = QImage(str(root / data['atlas']))
        if atlas.isNull():
            raise ValueError('Missing HD menu font atlas')
        self.glyphs = {char: (atlas.copy(*entry['rect']), entry)
                       for char, entry in data['glyphs'].items()}
        self.space = data['space_advance']

    def glyph(self, char):
        return self.glyphs.get(char, self.glyphs['?'])

    def advance(self, char):
        return self.space if char == ' ' else self.glyph(char)[1]['advance']

    def width(self, text, scale):
        text = text.upper()
        width = sum(self.advance(char) for char in text)
        if text and text[-1] != ' ':
            width += self.glyph(text[-1])[1]['width'] - self.advance(text[-1])
        return width * scale

    def draw(self, painter, text, center_x, center_y, scale=.65, limit=450):
        text = text.upper()
        if self.width(text, scale) > limit:
            while text and self.width(text + '...', scale) > limit:
                text = text[:-1]
            text += '...'
        cursor = center_x - self.width(text, scale) / 2
        for char in text:
            if char != ' ':
                image, entry = self.glyph(char)
                painter.drawImage(QRectF(cursor, center_y + (entry['top'] - 8) * scale,
                    entry['width'] * scale, entry['height'] * scale), image)
            cursor += self.advance(char) * scale
