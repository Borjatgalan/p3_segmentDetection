#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "QMessageBox"
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

/**
 * P3 - Segment Detection
 * Ivan González
 * Borja Alberto Tirado Galán
 *
 *
 */

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    cap = new VideoCapture(0);
    winSelected = false;

    //Inicializacion de imagenes
    colorImage.create(240,320,CV_8UC3);
    grayImage.create(240,320,CV_8UC1);
    destColorImage.create(240,320,CV_8UC3);
    destGrayImage.create(240,320,CV_8UC1);
    corners.create(240,320,CV_8UC1);

    visorS = new ImgViewer(&grayImage, ui->imageFrameS);
    visorD = new ImgViewer(&destGrayImage, ui->imageFrameD);


    connect(&timer,SIGNAL(timeout()),this,SLOT(compute()));
    connect(ui->captureButton,SIGNAL(clicked(bool)),this,SLOT(start_stop_capture(bool)));
    connect(ui->colorButton,SIGNAL(clicked(bool)),this,SLOT(change_color_gray(bool)));
    connect(visorS,SIGNAL(windowSelected(QPointF, int, int)),this,SLOT(selectWindow(QPointF, int, int)));
    connect(visorS,SIGNAL(pressEvent()),this,SLOT(deselectWindow()));

    connect(ui->loadButton,SIGNAL(pressed()),this,SLOT(loadFromFile()));
    connect(ui->showCorners_checkbox,SIGNAL(clicked()),this,SLOT(cornerDetection()));
    connect(ui->showCanny_checkbox,SIGNAL(clicked()),this,SLOT(edgesDetection()));
    connect(ui->showLines_checkbox,SIGNAL(clicked()),this,SLOT(linesDetection()));

    timer.start(30);


}

MainWindow::~MainWindow()
{
    delete ui;
    delete cap;
    delete visorS;
    delete visorD;
}

void MainWindow::compute()
{
    //Captura de imagen

    if(ui->captureButton->isChecked() && cap->isOpened())
    {
        *cap >> colorImage;

        cv::resize(colorImage, colorImage, Size(320, 240));
        cvtColor(colorImage, grayImage, COLOR_BGR2GRAY);
        cvtColor(colorImage, colorImage, COLOR_BGR2RGB);

    }


    //Procesamiento
    // Representar esquinas
    if(!cornerList.empty() && ui->showCorners_checkbox->isChecked())
        printCorners();

    // Volver a la imagen original cuando se desactiva la opcion Canny
    if((!ui->showCanny_checkbox->isChecked() && (!ui->showCorners_checkbox->isChecked() || !ui->showLines_checkbox->isChecked()))){

        grayImage.copyTo(destGrayImage);
    }
    // Representar lineas
    if(!lines.empty() && ui->showLines_checkbox->isChecked())
        printLines();

    if(winSelected)
    {
        visorS->drawSquare(QPointF(imageWindow.x+imageWindow.width/2, imageWindow.y+imageWindow.height/2), imageWindow.width,imageWindow.height, Qt::green );
    }
    visorS->update();
    visorD->update();

}


void MainWindow::start_stop_capture(bool start)
{
    if(start)
        ui->captureButton->setText("Stop capture");
    else
        ui->captureButton->setText("Start capture");
}

void MainWindow::change_color_gray(bool color)
{
    if(color)
    {
        ui->colorButton->setText("Gray image");
        visorS->setImage(&colorImage);
        visorD->setImage(&destColorImage);

    }
    else
    {
        ui->colorButton->setText("Color image");
        visorS->setImage(&grayImage);
        visorD->setImage(&destGrayImage);

    }
}

void MainWindow::selectWindow(QPointF p, int w, int h)
{
    QPointF pEnd;
    if(w>0 && h>0)
    {
        imageWindow.x = p.x()-w/2;
        if(imageWindow.x<0)
            imageWindow.x = 0;
        imageWindow.y = p.y()-h/2;
        if(imageWindow.y<0)
            imageWindow.y = 0;
        pEnd.setX(p.x()+w/2);
        if(pEnd.x()>=320)
            pEnd.setX(319);
        pEnd.setY(p.y()+h/2);
        if(pEnd.y()>=240)
            pEnd.setY(239);
        imageWindow.width = pEnd.x()-imageWindow.x;
        imageWindow.height = pEnd.y()-imageWindow.y;

        winSelected = true;
    }
}

void MainWindow::deselectWindow()
{
    winSelected = false;
}

void MainWindow::loadFromFile()
{
    disconnect(&timer,SIGNAL(timeout()),this,SLOT(compute()));

    Mat image;
    QString fileName = QFileDialog::getOpenFileName(this,tr("Open"), "/home",tr("Images (*.jpg *.png "
                                                                                "*.jpeg *.gif);;All Files(*)"));
    image = cv::imread(fileName.toStdString());

    if (fileName.isEmpty())
        return;
    else {
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::information(this, tr("Unable to open file"), file.errorString());
            return;
        }
        ui->captureButton->setChecked(false);
        ui->captureButton->setText("Start capture");
        cv::resize(image, colorImage, Size(320, 240));
        cvtColor(colorImage, colorImage, COLOR_BGR2RGB);
        cvtColor(colorImage, grayImage, COLOR_RGB2GRAY);

        if(ui->colorButton->isChecked())
            colorImage.copyTo(destColorImage);
        else
            grayImage.copyTo(destGrayImage);
        connect(&timer,SIGNAL(timeout()),this,SLOT(compute()));

    }

}

void MainWindow::saveToFile()
{
    disconnect(&timer,SIGNAL(timeout()),this,SLOT(compute()));
    Mat save_image;
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save Image File"),
                                                    QString(),
                                                    tr("JPG (*.JPG) ; jpg (*.jpg); png (*.png); jpeg(*.jpeg); gif(*.gif); All Files (*)"));
    if(ui->colorButton->isChecked())
        cvtColor(destColorImage, save_image, COLOR_RGB2BGR);

    else
        cvtColor(destGrayImage, save_image, COLOR_GRAY2BGR);

    if (fileName.isEmpty())
        return;
    else {
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly)) {
            QMessageBox::information(this, tr("Unable to open file"),
                                     file.errorString());
            return;
        }
    }
    cv::imwrite(fileName.toStdString(), save_image);

    connect(&timer,SIGNAL(timeout()),this,SLOT(compute()));
}

void MainWindow::cornerDetection()
{
    Mat dst;
    dst.create(240, 320, CV_32FC1);
    cornerList.clear();

    double harris_factor = ui->harrisFactor_box->value();
    int blockSize = ui->blockSize_box->value();
    float threshold = ui->threshold_box->value();

    dst = Mat::zeros(grayImage.size(), CV_32FC1 );

    //Metodo que calcula las esquinas
    cv::cornerHarris(grayImage, dst, blockSize, 3, harris_factor);

    //Almacenamiento de las esquinas en una lista
    for (int x=0; x < dst.cols; x++) {
        for (int y=0; y < dst.rows; y++) {
            if(dst.at<float>(y,x) > threshold)
            {
                punto p;
                p.point = Point(x,y);
                p.valor = dst.at<float>(y,x);
                this->cornerList.push_back(p);
            }
        }
    }
    //Lista ordenada de esquinas
    std::sort(cornerList.begin(), cornerList.end(), puntoCompare());

    //Supresion del no maximo
    for(int i=0; i < (int)cornerList.size(); i++){
        for(int j=i+1; j < (int)cornerList.size(); j++){
            if(abs(cornerList[i].point.x - cornerList[j].point.x) < threshold &&
                    abs(cornerList[i].point.y - cornerList[j].point.y) < threshold){

                cornerList.erase(cornerList.begin()+j);
                j--;
            }
        }
    }


    dst.copyTo(destGrayImage);
}

void MainWindow::printCorners()
{
    for (int i = 0;i < (int) cornerList.size(); i++) {
        visorD->drawEllipse(QPoint(cornerList[i].point.x, cornerList[i].point.y), 2, 2, Qt::red);
    }

}

void MainWindow::edgesDetection()
{
    Mat detected_edges;
    int lowThreshold = ui->minthreshold_box->value();
    int const maxThreshold = ui->maxthreshold_box->value();
    int kernel_size = 3;

    grayImage.copySize(destGrayImage);
    // Reduce noise with a kernel 3x3
    blur(destGrayImage, detected_edges, Size(3,3));

    // Canny detector
    cv::Canny(detected_edges, detected_edges, lowThreshold, maxThreshold, kernel_size);

    // Using Canny's output as a mask, we display our result
    destGrayImage = Scalar::all(0);

    grayImage.copyTo(destGrayImage, detected_edges);

}

void MainWindow::linesDetection()
{
    threshold = ui->linesthreshold_box->value();
    rho = ui->rhoresolution_box->value();
    theta = ui->thetaresolution_box->value();
    //HoughLines nos devuelve los parámetros de la linea
    //Pero necesitamos los limites de la linea
    //cv::HoughLines(destGrayImage, lines, rho, theta, threshold, 0,0,0, CV_PI/180);

    QPoint p1, p2;
    HoughLinesP( destGrayImage, lines, 1, CV_PI/180, 80, 30, 10 );
    for( size_t i = 0; i < lines.size(); i++ )
    {
        line( destGrayImage, Point(lines[i][0], lines[i][1]),
            Point(lines[i][2], lines[i][3]), Scalar(0,0,255), 3, 8 );
    }
    //Casos especiales
    /*for ( size_t i = 0; i < lines.size(); i++ )     {
        int x1 = lines[i][0];
        int y1 = lines[i][1];
        int x2 = lines[i][2];
        int y2 = lines[i][3];

        //TODO: Descartar los puntos fuera de rango

        // === Casos Especiales ===
        //calculo de la linea vertical
        //Es decir, si sin(theta) == 0
        if(sin(theta) == 0){
            x1=rho / cos(theta);
            y1=0;
            x2=rho / cos(theta);
            y2=239;
        }
        //Si la linea es horizontal
        if(cos(theta) == 0){
            x1=0;
            y1=rho / sin(theta);
            x2=319;
            y2=rho / sin(theta);
        }
        p1.setX(x1);
        p1.setY(y1);
        p2.setX(x2);
        p2.setY(y2);
        qLines1.push_back(p1);
        qLines2.push_back(p2);

    }*/
}


void MainWindow::printLines(){

    for(size_t i=0;i < qLines1.size();i++)
     visorD->drawLine(QLine(qLines1[i],qLines2[i]), Qt::green, 2);

}




