import numpy as np
import cv2
import argparse
import os
import math
import sys
import copy
import MainReactor # Requires MainReactor build on your machine to work!
from queue import Queue

import PyQt5.QtCore
import PyQt5.QtWidgets

from PyQt5.QtWidgets import *
from PyQt5.QtCore import *
from PyQt5.QtGui import *

import matplotlib
import matplotlib.pyplot as plt
from matplotlib.backends.backend_qtagg import FigureCanvas
from matplotlib.figure import Figure
from matplotlib import ticker

matplotlib.use('agg')
numBins = 40

class Window(QMainWindow):
    onTotalAbHistogramPixmapSignal = pyqtSignal(QPixmap)

    """Main Window."""
    def __init__(self, parent=None):
        """Initializer."""
        super().__init__(parent)
        self.setWindowTitle("Histogram Creation")
        self.createUI()
        self.createActions()
        self.createMenuBar()
        self.createToolBar()
        self.createUndo()

    def createUI(self):
        self.stack = QStackedWidget()

        self.selectImageButton = QPushButton()
        self.selectImageButton.setText("Open Image")
        self.selectImageButton.clicked.connect(self.onOpen)

        self.tabLayout = QHBoxLayout()
        self.images = []
        self.imageNamesModel = QStringListModel()
        self.imageNamesList = ImageListView(self)
        self.imageNamesList.setSizePolicy(QSizePolicy.Minimum, QSizePolicy.Minimum)
        self.imageReader = ImageReader()
        self.imageReader.imageReadSignal.connect(self.onImageRead)
        self.imageReader.start()
        self.imageMaskedHistogramCalculator = ImageMaskedHistogramCalculator(self.images)
        self.imageMaskedHistogramCalculator.calculatedMaskedHistogramSignal.connect(self.onCalculateMaskedHistogram)
        self.imageMaskedHistogramCalculator.start()
        self.maskedAbHist = None
        self.lastMaskedAbHistPixmap = None

        self.imageNamesList.setModel(self.imageNamesModel)
        self.imageNamesList.selectionModel().selectionChanged.connect(self.onImageSelectionChanged)
        self.tabs = QTabWidget(self)
        self.tabs.findChild(QTabBar).hide()
        self.tabs.setTabsClosable(True)
        self.tabs.tabCloseRequested.connect(self.onTabClosed)
        self.tabs.currentChanged.connect(self.onTabChanged)
        self.tabLayout.addWidget(self.imageNamesList)
        self.tabLayout.addWidget(self.tabs)
        self.tabWidget = QWidget()
        self.tabWidget.setLayout(self.tabLayout)

        self.stack.addWidget(self.selectImageButton)
        self.stack.addWidget(self.tabWidget)
        self.setCentralWidget(self.stack)

    def createMenuBar(self):
        menuBar = QMenuBar(self)
        self.setMenuBar(menuBar)
        fileMenu = menuBar.addMenu("&File")
        fileMenu.addAction(self.openAction)
        fileMenu.addAction(self.saveAction)
        fileMenu.addAction(self.closeAction)
        fileMenu.addAction(self.cameraAction)
        fileMenu.addAction(self.clearAction)

    def createToolBar(self):
        fileToolBar = self.addToolBar("File")
        fileToolBar.addAction(self.openAction)
        fileToolBar.addAction(self.saveAction)
        fileToolBar.addAction(self.clearAction)
        fileToolBar.addAction(self.closeAction)
        fileToolBar.addAction(self.cameraAction)

    def createActions(self):
        self.openAction = QAction("&Open Image...", self)
        self.openAction.triggered.connect(self.onOpen)
        self.clearAction = QAction("Clear Image", self)
        self.clearAction.triggered.connect(self.onClear)
        self.saveAction = QAction("&Save", self)
        self.saveAction.triggered.connect(self.onSave)
        self.closeAction = QAction("&Close", self)
        self.closeAction.triggered.connect(self.onClose)
        self.cameraAction = QAction("&Open Camera", self)
        self.cameraAction.triggered.connect(self.onCamera)

    def createUndo(self):
        self.undoStack = QUndoStack(self)
        self.undoAction = self.undoStack.createUndoAction(self, self.tr("Undo"))
        self.undoAction.setShortcuts(QKeySequence.Undo)
        self.redoAction = self.undoStack.createRedoAction(self, self.tr("Redo"))
        self.redoAction.setShortcuts(QKeySequence.Redo)
        self.addAction(self.undoAction)
        self.addAction(self.redoAction)

    def onOpen(self):
        path = QFileDialog.getOpenFileName(self, "Open Image", QDir.currentPath(), "Image Files (*.png *.jpg *.bmp)")[0]
        if path:
            self.openImage(path)
    
    def openImage(self, path):
        foundImage = False
        for image in self.images:
            if image.path == path:
                existingImage = self.images
                self.tabs.setCurrentWidget(self.tabs.findChild(QWidget, path))
                foundImage = True
                break
        
        if not foundImage:
            self.imageReader.queue.put(path)

    def onImageRead(self, newImage):
        newImage.maskUpdatedSignal.connect(self.onUpdateMask)
        self.images.append(newImage)
        self.refreshImageList()
        newImage.clearMask()

        scrollArea = QScrollArea()
        scrollArea.setWidget(ImageTab(self, newImage))
        scrollArea.setWidgetResizable(True)
        tab = self.tabs.addTab(scrollArea, newImage.path)
        self.tabs.setCurrentWidget(scrollArea)
        self.stack.setCurrentIndex(1)

        if self.lastMaskedAbHistPixmap is not None:
            self.onTotalAbHistogramPixmapSignal.emit(self.lastMaskedAbHistPixmap)
            newImage.createBackprojectPreview(self.maskedAbHist)

        self.undoStack.clear()

    def onTabClosed(self, index):
        del self.images[index]
        self.refreshImageList()
        self.undoStack.clear()
        self.onUpdateMask(None)

        if self.tabs.count() == 0:
            self.maskedAbHist = None
            self.stack.setCurrentIndex(0)

    def onTabChanged(self):
        currentTab = self.tabs.currentWidget()
        if currentTab is not None:
            index = self.tabs.currentIndex()
            modelIndex = self.imageNamesModel.createIndex(index, 0)
            self.imageNamesList.selectionModel().select(modelIndex, QItemSelectionModel.Select)

    def onImageSelectionChanged(self):
        selectedIndexes = self.imageNamesList.selectionModel().selectedIndexes()
        if len(selectedIndexes) > 0:
            index = selectedIndexes[0].row()
            self.tabs.setCurrentIndex(index)

    def onImageDeletePressed(self):
        selectedIndexes = self.imageNamesList.selectionModel().selectedIndexes()
        if len(selectedIndexes) > 0:
            index = selectedIndexes[0].row()
            self.tabs.removeTab(index)
            self.onTabClosed(index)

            if self.tabs.count() > 0:
                self.tabs.setCurrentIndex(0 if index == 0 else index - 1)
                self.onTabChanged()

    def onSave(self):
        if self.maskedAbHist is not None:
            fileName = QFileDialog.getSaveFileName(self, "Save Histogram", QDir.currentPath(), "Numpy Files (*.npy)")[0]
            np.save(fileName, self.maskedAbHist)

    def onClose(self):
        if self.tabs.currentWidget() is not None:
            index = self.tabs.currentIndex()
            self.tabs.removeTab(index)
            self.onTabClosed(index)

            if self.tabs.count() > 0:
                self.tabs.setCurrentIndex(0 if index == 0 else index - 1)
                self.onTabChanged()

    def onCamera(self):
        self.cameraWindow = CameraWindow(maskedAbHist=self.maskedAbHist)
        self.cameraWindow.frameSignal.connect(self.onFrameReceived)
        self.cameraWindow.show()

    def onClear(self):
        currentTab = self.tabs.currentWidget()
        if currentTab is not None:
            currentTab.widget().clearMask()
            self.undoStack.clear()

    def onUpdateMask(self, image):
        self.imageMaskedHistogramCalculator.queue.put(copy.copy(self.images))

    def onCalculateMaskedHistogram(self, result):
        (maskedAbHist, maskedAbHistPixmap) = result
        self.maskedAbHist = maskedAbHist
        self.lastMaskedAbHistPixmap = maskedAbHistPixmap
        self.onTotalAbHistogramPixmapSignal.emit(maskedAbHistPixmap)

    def refreshImageList(self):
        self.imageNamesModel.setStringList([os.path.basename(image.path) for image in self.images])
        self.imageNamesList.setMaximumWidth(self.imageNamesList.sizeHintForColumn(0))

    def onFrameReceived(self, frame):
        self.imageWriter = ImageWriter(frame)
        self.imageWriter.imageWriteSignal.connect(self.onImageWrite)
        self.imageWriter.start()

    def onImageWrite(self, path):
        self.openImage(path)

    def closeEvent(self, event):
        self.imageReader.queue.put(None)
        self.imageMaskedHistogramCalculator.queue.put(None)
        event.accept()

class Image(QObject):
    maskUpdatedSignal = pyqtSignal(QObject)
    backprojectPixmapSignal = pyqtSignal(QPixmap)

    def __init__(self, path):
        QObject.__init__(self)
        self.path = path
        self.bgr = cv2.imread(path)
        self.hsv = cv2.cvtColor(self.bgr, cv2.COLOR_BGR2HSV)
        self.lab = cv2.cvtColor(self.bgr, cv2.COLOR_BGR2LAB)
        self.mask = np.zeros(self.bgr.shape[:2], dtype=np.uint8)
        self.renderMask = QImage(self.bgr.shape[1], self.bgr.shape[0], QImage.Format_ARGB32)
        self.pixmap = self.createPixmap(self.bgr)
        self.abHist = cv2.calcHist(cv2.split(self.lab), [1, 2], None, [numBins, numBins], [0, 256, 0, 256])
        self.abHist = cv2.normalize(self.abHist, self.abHist, 0, 256, norm_type = cv2.NORM_MINMAX)
        self.abHistPixmap = self.createLabHistPixmap()
        self.clearMask()
        self.calcMaskedHist()

    def calcMaskedHist(self):
        self.maskedAbHist = cv2.calcHist(cv2.split(self.lab), [1, 2], self.mask, [numBins, numBins], [0, 256, 0, 256])

    def createPixmap(self, frame):
        height, width, _ = frame.shape
        bytesPerLine = 3 * width
        return QPixmap(QImage(frame.data, width, height, bytesPerLine, QImage.Format_BGR888))

    def updateMask(self, rect, val):
        top = rect.top()
        left = rect.left()
        height = rect.height()
        width = rect.width()
        self.mask[top:top + height, left:left + width] = val

        painter = QPainter(self.renderMask)
        painter.setCompositionMode(QPainter.CompositionMode_Source)
        painter.setPen(Qt.NoPen)
        painter.setBrush(QColor(255, 0, 0, min(val, 50)))
        painter.drawRect(rect)
        self.maskUpdatedSignal.emit(self)

    def clearMask(self):
        self.mask.fill(0)
        self.renderMask.fill(QColor(0, 0, 0, 0))
        self.maskUpdatedSignal.emit(self)

    def createLabHistPixmap(self):
        fig, ab_ax = plt.subplots(1, 1, figsize=(5, 4), dpi=80)

        ab_ax.set_xlim([0, numBins - 1])
        ab_ax.set_ylim([0, numBins - 1])

        ab_ax.set_xlabel('Channel A')
        ab_ax.set_ylabel('Channel B')
        ab_ax.set_title('A and B Histogram')

        ab_ax.yaxis.set_major_locator(
            ticker.MultipleLocator(5))
        ab_ax.xaxis.set_major_locator(
            ticker.MultipleLocator(5))
        ab_im = ab_ax.imshow(self.abHist)
        fig.colorbar(ab_im, ax=ab_ax)
        plt.tight_layout()

        return pixmapFromFig(fig)

    def createBackprojectPreview(self, maskedAbHist):
        if maskedAbHist is not None:
            self.maskedAbHist = maskedAbHist
            backproj = cv2.calcBackProject([self.lab], [1, 2], self.maskedAbHist, [0, 256, 0, 256], scale=1)
            backproj = cv2.cvtColor(backproj, cv2.COLOR_GRAY2BGR)
            self.backprojectPixmap = self.createPixmap(backproj)
            self.backprojectPixmapSignal.emit(self.backprojectPixmap)


class ImageWriter(QThread):
    imageWriteSignal = pyqtSignal(str)

    def __init__(self, image):
        QThread.__init__(self)
        self.image = image

    def run(self):
        picsDirectory = 'pics'
        if not(os.path.exists(picsDirectory)):
            os.makedirs(picsDirectory)

        picNames = [os.path.splitext(f)[0] for f in os.listdir(picsDirectory) if os.path.isfile(os.path.join(picsDirectory, f))]
        picNumbers = sorted([int(f) for f in picNames if f.isdigit()])
        largestPicNumber = picNumbers[-1] + 1 if len(picNumbers) > 0 else 0
        imagePath = os.path.join(picsDirectory, str(largestPicNumber) + '.png')
        cv2.imwrite(imagePath, self.image)
        self.imageWriteSignal.emit(imagePath)

class ImageReader(QThread):
    imageReadSignal = pyqtSignal(Image)

    def __init__(self):
        QThread.__init__(self)
        self.queue = Queue()

    def run(self):
        for path in iter(self.queue.get, None):
            image = Image(path)
            self.imageReadSignal.emit(image)

            self.queue.task_done()

class ImageMaskedHistogramCalculator(QThread):
    calculatedMaskedHistogramSignal = pyqtSignal(tuple)

    def __init__(self, images):
        QThread.__init__(self)
        self.queue = Queue()
        self.images = images

    def run(self):
        for images in iter(self.queue.get, None):
            hist = None
            for image in images:
                image.calcMaskedHist()
                if hist is not None:
                    hist = cv2.add(hist, image.maskedAbHist)
                else:
                    hist = image.maskedAbHist

            if hist is not None:
                maskedAbHist = cv2.normalize(hist, hist, 0, 256, norm_type = cv2.NORM_MINMAX)
                maskedAbHistPixmap = self.createMaskedAbHistPixmap(maskedAbHist)
                self.calculatedMaskedHistogramSignal.emit((maskedAbHist, maskedAbHistPixmap))
                for image in self.images:
                    image.createBackprojectPreview(maskedAbHist)

    def createMaskedAbHistPixmap(self, maskedAbHist):
        fig, ab_ax = plt.subplots(1, 1, figsize=(5, 4), dpi=80)

        ab_ax.set_xlim([0, numBins - 1])
        ab_ax.set_ylim([0, numBins - 1])

        ab_ax.set_xlabel('Channel A')
        ab_ax.set_ylabel('Channel B')
        ab_ax.set_title('Total A and B Histogram')

        ab_ax.yaxis.set_major_locator(
            ticker.MultipleLocator(5))
        ab_ax.xaxis.set_major_locator(
            ticker.MultipleLocator(5))
        ab_im = ab_ax.imshow(maskedAbHist)
        fig.colorbar(ab_im, ax=ab_ax)
        plt.tight_layout()

        return pixmapFromFig(fig)

class ImageHistogramRenderer(QThread):
    renderedHistogramSignal = pyqtSignal(QPixmap)

    def __init__(self):
        QThread.__init__(self)
        self.queue = Queue()
    
    def run(self):
        for imageTab in iter(self.queue.get, None):
            pixmap = imageTab.renderImageLabHistograms()
            #self.renderedHistogramSignal.emit(pixmap)   

class ImageListView(QListView):
    def __init__(self, parent):
        QListView.__init__(self)
        self.parent = parent
        self.setEditTriggers(QAbstractItemView.NoEditTriggers)

    def keyPressEvent(self, event):
        key = event.key()
        if key == Qt.Key_Delete or key == Qt.Key_Backspace:
            self.parent.onImageDeletePressed()


class ImageTab(QWidget):
    def __init__(self, parent, image):
        QWidget.__init__(self)
        self.parent = parent
        self.image = image
        self.parent.onTotalAbHistogramPixmapSignal.connect(self.onTotalAbHistogramReceived)
        self.image.backprojectPixmapSignal.connect(self.onBackprojectPixmapReceived)

        self.imageLayout = QVBoxLayout()
        self.imageLayout.setContentsMargins(0, 0, 0, 0)
        self.imageLayout.setSpacing(0)
        self.imageWidget = QWidget()
        self.imageWidget.setLayout(self.imageLayout)
        self.imageLabel = PixmapImageLabel(parent=self, pixmap=self.image.pixmap, mask=self.image.renderMask, selectable=True)
        self.backprojectPreviewLabel = PixmapImageLabel(parent=self, pixmap=QPixmap(), selectable=False)
        self.imageLayout.addWidget(self.imageLabel)
        self.imageLayout.addWidget(self.backprojectPreviewLabel)

        self.histogramLayout = QVBoxLayout()
        self.histogramLayout.setContentsMargins(0, 0, 0, 0)
        self.histogramLayout.setSpacing(0)
        self.histogramWidget = QWidget()
        self.histogramWidget.setLayout(self.histogramLayout)
        self.imageAbHistogramLabel = PixmapImageLabel(parent=self, pixmap=self.image.abHistPixmap, selectable=False)
        self.totalAbHistogramLabel = PixmapImageLabel(parent=self, pixmap=QPixmap(), selectable=False)
        self.histogramLayout.addWidget(self.imageAbHistogramLabel)
        self.histogramLayout.addWidget(self.totalAbHistogramLabel)


        self.layout = QGridLayout()
        self.layout.setSizeConstraint(QLayout.SetFixedSize)
        self.layout.addWidget(self.imageWidget, 1, 1, Qt.AlignTop)
        self.layout.addWidget(self.histogramWidget, 1, 2, Qt.AlignTop)

        self.setLayout(self.layout)

    def changeMask(self, rect, val):
        self.image.updateMask(rect, val)

    def clearMask(self):
        self.image.clearMask()

    def onTotalAbHistogramReceived(self, pixmap):
        self.totalAbHistogramLabel.setPixmap(pixmap)
        self.totalAbHistogramLabel.setFixedWidth(pixmap.width())
        self.totalAbHistogramLabel.setFixedHeight(pixmap.height())

    def onBackprojectPixmapReceived(self, pixmap):
        self.backprojectPreviewLabel.setPixmap(pixmap)
        self.backprojectPreviewLabel.setFixedWidth(pixmap.width())
        self.backprojectPreviewLabel.setFixedHeight(pixmap.height())

class PixmapImageLabel(QLabel):
    def __init__(self, parent, pixmap, mask=None, selectable=True):
        QLabel.__init__(self)
        self.selectable = selectable
        self.mask = mask
        self.parent = parent
        self.parent.image.maskUpdatedSignal.connect(self.onUpdateMask)
        self.setPixmap(pixmap)
        self.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed)
        self.setFixedWidth(pixmap.width())
        self.setFixedHeight(pixmap.height())
        self.selectedRect = None
        if selectable:
            self.setMouseTracking(True)

    def paintEvent(self, event):
        QLabel.paintEvent(self, event)
        painter = QPainter(self)
        painter.setPen(Qt.red)
        if self.selectedRect is not None:
            painter.drawRect(self.selectedRect)
        if self.mask is not None:
            painter.drawImage(0, 0, self.mask)

    def mousePressEvent(self, event):
        if self.selectable:
            x = event.x()
            y = event.y()
            self.origX = x
            self.origY = y
            self.selectedRect = QRect(x, y, 0, 0)
            self.update()

    def mouseReleaseEvent(self, event):
        if self.selectable and self.selectedRect is not None:
            self.parent.parent.undoStack.push(PixelChangeCommand(self.parent, self.selectedRect))
            self.selectedRect = None
            self.update()

    def mouseMoveEvent(self, event):
        if self.selectable and self.selectedRect is not None:
            x = event.x()
            y = event.y()
            self.selectedRect.setCoords(min(self.origX, x), min(self.origY, y), max(self.origX, x), max(self.origY, y))
            self.update()

    def onUpdateMask(self, image):
        self.update()

class PixelChangeCommand(QUndoCommand):
    def __init__(self, imageWidget, rect):
        QUndoCommand.__init__(self)
        self.imageWidget = imageWidget
        self.rect = rect

    def undo(self):
        self.imageWidget.changeMask(self.rect, 0)

    def redo(self):
        self.imageWidget.changeMask(self.rect, 255)

class CameraWindow(QWidget):
    frameSignal = pyqtSignal(np.ndarray)

    def __init__(self, parent=None, maskedAbHist=None):
        super().__init__(parent)
        self.maskedAbHist = maskedAbHist
        self.setWindowTitle("Camera")
        self.createUI()
        self.setupCameraWorker()

    def createUI(self):
        self.imageLabel = QLabel()
        self.imageLabel.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed)
        self.imageLabel.setScaledContents(False)
        self.backprojectLabel = QLabel()
        self.backprojectLabel.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed)
        self.backprojectLabel.setScaledContents(False)
        self.pictureButton = QPushButton()
        self.pictureButton.setText("Take Picture")
        self.pictureButton.clicked.connect(self.onPicButtonPressed)
        self.layout = QGridLayout()
        self.layout.setSizeConstraint(QLayout.SetFixedSize)
        self.layout.addWidget(self.imageLabel, 1, 1, Qt.AlignTop)
        self.layout.addWidget(self.pictureButton, 1, 2, Qt.AlignCenter)
        self.layout.addWidget(self.backprojectLabel, 2, 1, Qt.AlignTop)
        self.setLayout(self.layout)

    def setupCameraWorker(self):
        self.lastFrame = None
        self.cameraWorker = CameraWorker(maskedAbHist=self.maskedAbHist)
        self.cameraWorker.frameSignal.connect(self.onFrameReceived)
        self.cameraWorker.framePixmapSignal.connect(self.onFramePixmapReceived)
        self.cameraWorker.backprojectPixmapSignal.connect(self.onBackprojectPixmapReceived)
        self.cameraWorker.start()

    def onFrameReceived(self, frame):
        self.lastFrame = frame

    def onFramePixmapReceived(self, pixmap):
        self.imageLabel.setPixmap(pixmap)

    def onBackprojectPixmapReceived(self, pixmap):
        self.backprojectLabel.setPixmap(pixmap)

    def onPicButtonPressed(self):
        if self.lastFrame is not None:
            self.frameSignal.emit(self.lastFrame)

    def closeEvent(self, event):
        self.cameraWorker.interrupt()
        event.accept()

class CameraWorker(QThread):
    frameSignal = pyqtSignal(np.ndarray)
    framePixmapSignal = pyqtSignal(QPixmap)
    backprojectPixmapSignal = pyqtSignal(QPixmap)

    def __init__(self, maskedAbHist=None):
        QThread.__init__(self)
        self.sharedMat = MainReactor.SharedMat("RS_COLOR_0")
        self.maskedAbHist = maskedAbHist
        self.active = True
    
    def run(self):
        while self.active:
            self.sharedMat.waitForFrame()
            frame = np.array(self.sharedMat.mat, copy=True)                
            pixmap = self.pixmapFromFrame(frame)
            if self.maskedAbHist is not None:
                self.backprojectPixmapSignal.emit(self.backprojectPreviewFromFrame(frame))
            self.frameSignal.emit(frame)
            self.framePixmapSignal.emit(pixmap)

    def interrupt(self):
        self.active = False

    def backprojectPreviewFromFrame(self, frame):
        labFrame = cv2.cvtColor(frame, cv2.COLOR_BGR2LAB)
        backproj = cv2.calcBackProject([labFrame], [1, 2], self.maskedAbHist, [0, 256, 0, 256], scale=1)
        backproj = cv2.cvtColor(backproj, cv2.COLOR_GRAY2BGR)
        return self.pixmapFromFrame(backproj)

    def pixmapFromFrame(self, frame, format=QImage.Format_BGR888):
        scale_percent = 20
        width = int(frame.shape[1] * scale_percent / 100)
        height = int(frame.shape[0] * scale_percent / 100)
        frame = cv2.resize(frame, (width, height), interpolation = cv2.INTER_AREA)
        bytesPerLine = 3 * width
        return QPixmap(QImage(frame, width, height, bytesPerLine, format))

def pixmapFromFig(fig):
    fig.canvas.draw()
    data = np.frombuffer(fig.canvas.tostring_rgb(), dtype=np.uint8)
    data = data.reshape(fig.canvas.get_width_height()[::-1] + (3,))
    height, width, channel = data.shape
    bytesPerLine = 3 * width
    qtimg = QImage(data, width, height, bytesPerLine, QImage.Format_RGB888)
    pixmap = QPixmap(qtimg)
    plt.close()
    return pixmap

def main():
    app = QApplication(sys.argv)
    main = Window()
    main.show()
    sys.exit(app.exec_())

if __name__ == '__main__':
    main()
