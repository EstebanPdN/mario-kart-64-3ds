#!/usr/bin/env python3
"""Mario Kart 64 desktop extractor, GUI and command line."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import sys
import threading
import time
from core import Cancelled, convert


def create_window():
    from PySide6.QtCore import Qt, QThread, Signal, QEvent
    from PySide6.QtGui import QPainter, QIcon, QCursor, QGuiApplication
    from PySide6.QtWidgets import QFileDialog, QMessageBox, QWidget, QPushButton
    from pixel_ui import MenuArt

    class Worker(QThread):
        changed = Signal(str, int)
        succeeded = Signal(str)
        failed = Signal(str)
        cancelled = Signal()

        def __init__(self, source, output, replace):
            super().__init__()
            self.source, self.output, self.replace = source, output, replace
            self.cancel_event = threading.Event()

        def run(self):
            last_time, last_percent = 0, -1

            def progress(phase, percent):
                nonlocal last_time, last_percent
                now = time.monotonic()
                if percent > last_percent or now-last_time > .1:
                    self.changed.emit(phase, percent)
                    last_time, last_percent = now, percent
            try:
                result = convert(self.source, self.output, progress, self.cancel_event, replace=self.replace)
                self.succeeded.emit(result['path'])
            except Cancelled:
                self.cancelled.emit()
            except Exception as error:
                self.failed.emit(str(error))

    class Window(QWidget):
        def __init__(self):
            super().__init__()
            self.setWindowTitle('MK64 Extractor')
            screen = QGuiApplication.screenAt(QCursor.pos()) or QGuiApplication.primaryScreen()
            available = screen.availableGeometry()
            side = min(512, max(1, int(min(available.width(), available.height()) * .8)))
            self.setFixedSize(side, side)
            self.move(available.center().x() - side // 2,
                      available.center().y() - side // 2)
            self.setAcceptDrops(True)
            self.setWindowIcon(QIcon(str(Path(__file__).with_name('assets') / 'icon.png')))
            self.art = MenuArt()
            self.worker = None
            self.closing = False
            self.percent = 0
            self.phase = 'DROP ROM HERE'
            self.filename = ''
            self.ready = self.was_cancelled = self.failed = False
            self.footer_button = QPushButton('CHOOSE ROM', self)
            self.layout_action_button()
            self.footer_button.setAccessibleName('Choose ROM')
            self.footer_button.setCursor(Qt.CursorShape.PointingHandCursor)
            self.footer_button.setStyleSheet('QPushButton { border: none; background: transparent; color: transparent; }')
            self.footer_button.installEventFilter(self)
            self.footer_button.clicked.connect(self.footer_action)
            self.setAccessibleName('Mario Kart 64 game data extractor')

        def layout_action_button(self):
            # Match the painted action tile at every supported startup size.
            scale = self.width() / 320
            self.footer_button.setGeometry(round(66 * scale), round(218 * scale),
                                           round(188 * scale), round(38 * scale))

        def resizeEvent(self, event):
            if hasattr(self, 'footer_button'):
                self.layout_action_button()
            super().resizeEvent(event)

        def eventFilter(self, watched, event):
            if event.type() in (QEvent.Type.FocusIn, QEvent.Type.FocusOut):
                self.update()
            return super().eventFilter(watched, event)

        def paintEvent(self, event):
            image = self.art.render(self.phase, self.percent, self.worker is not None,
                self.ready, self.filename, self.was_cancelled, self.failed, self.footer_button.hasFocus())
            painter = QPainter(self)
            painter.setRenderHint(QPainter.RenderHint.SmoothPixmapTransform, True)
            painter.drawImage(self.rect(), image)
            painter.end()

        def footer_action(self):
            if self.worker is None:
                self.pick()
            else:
                self.cancel()

        def keyPressEvent(self, event):
            if event.key() == Qt.Key.Key_Escape and self.worker is not None:
                self.cancel()
            elif event.key() in (Qt.Key.Key_Return, Qt.Key.Key_Enter) and self.worker is None:
                self.pick()
            else:
                super().keyPressEvent(event)

        def dragEnterEvent(self, event):
            urls = event.mimeData().urls()
            if self.worker is None and len(urls) == 1 and urls[0].isLocalFile() and Path(urls[0].toLocalFile()).suffix.lower() in ('.z64', '.n64', '.v64'):
                event.acceptProposedAction()

        def dropEvent(self, event):
            if self.worker is None and event.mimeData().hasUrls():
                urls = event.mimeData().urls()
                if len(urls) == 1 and urls[0].isLocalFile():
                    self.begin_conversion(urls[0].toLocalFile())
                    event.acceptProposedAction()

        def pick(self):
            source, _ = QFileDialog.getOpenFileName(self, 'Choose your USA Mario Kart 64 ROM', '',
                'Nintendo 64 ROM (*.z64 *.n64 *.v64 *.Z64 *.N64 *.V64);;All files (*)')
            if source:
                self.begin_conversion(source)

        def begin_conversion(self, source):
            if self.worker is not None:
                return
            output, _ = QFileDialog.getSaveFileName(self, 'Save Mario Kart 64 game data',
                str(Path(source).parent / 'mk64.o2r'), 'MK64 game data (*.o2r)',
                options=QFileDialog.Option.DontConfirmOverwrite)
            if not output:
                return
            if Path(output).suffix.lower() != '.o2r':
                output += '.o2r'
            replace = Path(output).exists()
            if replace and QMessageBox.question(self, 'Replace game data?',
                'Replace this existing file after extraction has finished and been verified?') != QMessageBox.StandardButton.Yes:
                return
            self.percent = 0
            self.phase = 'Checking ROM'
            self.filename = Path(source).name
            self.ready = self.was_cancelled = self.failed = False
            self.worker = Worker(source, output, replace)
            self.worker.changed.connect(self.progress_changed)
            self.worker.succeeded.connect(self.conversion_succeeded)
            self.worker.failed.connect(self.conversion_failed)
            self.worker.cancelled.connect(self.conversion_cancelled)
            self.worker.finished.connect(self.worker_finished)
            self.footer_button.setText('CANCEL')
            self.footer_button.setAccessibleName('Cancel extraction')
            self.worker.start()
            self.update()

        def progress_changed(self, phase, percent):
            if self.phase != 'CANCELLING':
                self.phase, self.percent = phase, max(self.percent, percent)
            self.setAccessibleDescription(f'{self.phase}: {self.percent} percent')
            self.update()

        def conversion_succeeded(self, output):
            self.percent, self.phase, self.ready = 100, 'READY!', True
            self.filename = Path(output).name
            self.update()

        def conversion_failed(self, message):
            self.phase, self.failed, self.percent = 'ROM ERROR', True, 0
            self.update()
            if not self.closing:
                QMessageBox.warning(self, 'Extraction failed', message)

        def conversion_cancelled(self):
            self.phase, self.percent, self.was_cancelled = 'DROP ROM HERE', 0, True
            self.update()

        def cancel(self):
            if self.worker is not None:
                self.worker.cancel_event.set()
                self.phase = 'CANCELLING'
                self.update()

        def worker_finished(self):
            previous, self.worker = self.worker, None
            self.footer_button.setText('CHOOSE ROM')
            self.footer_button.setAccessibleName('Choose ROM')
            previous.deleteLater()
            self.update()
            if self.closing:
                self.close()

        def closeEvent(self, event):
            if self.worker is not None:
                self.closing = True
                self.cancel()
                event.ignore()
            else:
                event.accept()

    return Window()


def main(argv=None):
    parser = argparse.ArgumentParser(description='Create mk64.o2r from your USA Mario Kart 64 ROM.')
    parser.add_argument('--rom', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args(argv)
    if args.rom is not None or args.output is not None:
        if args.rom is None or args.output is None:
            parser.error('--rom and --output are required together')
        try:
            print(json.dumps(convert(args.rom, args.output)))
            return 0
        except (Exception, KeyboardInterrupt) as error:
            print(str(error) or 'Cancelled.', file=sys.stderr)
            return 1
    from PySide6.QtWidgets import QApplication
    application = QApplication(sys.argv[:1])
    application.setApplicationName('MK64 Extractor')
    window = create_window()
    window.show()
    return application.exec()


if __name__ == '__main__':
    raise SystemExit(main())
