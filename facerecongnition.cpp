#include "facerecongnition.h"
#include "ui_facerecongnition.h"
#include "ui_InformationEntry.h"
#include "ui_setTime.h"
InformationEntry::InformationEntry(QWidget*parent):QWidget(parent),uiInf(new Ui::InformationEntry){
    uiInf->setupUi(this);
    connect(uiInf->entering,&QPushButton::clicked,this,[=](){
        emit entrySuccess();
    });
}

Ui::InformationEntry* InformationEntry::getUi(){
    return uiInf;
}

InformationEntry::~InformationEntry(){
    delete uiInf;
}

setTime::setTime(QString account,QWidget*parent):QWidget(parent),uiSet(new Ui::setTime){
    uiSet->setupUi(this);
    uiSet->info->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter);
    uiSet->info->setColumnWidth(0,60);
    uiSet->info->setColumnWidth(1,150);
    //uiSet->info->setColumnWidth(2,60);
    uiSet->info->horizontalHeader()->setStretchLastSection(true);

    QVector<std::tuple<QString,QString>> info = DataBase::instance()->getclassScheduleInfo(account);
    for(std::tuple<QString,QString> t:info){
        int rowCount = uiSet->info->rowCount();
        uiSet->info->insertRow(rowCount);
        uiSet->info->setItem(rowCount,0,new QTableWidgetItem(account));
        uiSet->info->setItem(rowCount,1,new QTableWidgetItem(std::get<0>(t)));
        uiSet->info->setItem(rowCount,2,new QTableWidgetItem(std::get<1>(t)));
    }

    connect(uiSet->add,&QPushButton::clicked,this,[=](){
        int rowCount = uiSet->info->rowCount();
        uiSet->info->insertRow(rowCount);
        uiSet->info->setItem(rowCount,0,new QTableWidgetItem(account));
    });
    connect(uiSet->remove,&QPushButton::clicked,this,[=](){
        QList<QTableWidgetItem*> items = uiSet->info->selectedItems();
        if(items.size() > 0){
            int row = uiSet->info->row(items[0]);
            uiSet->info->removeRow(row);
        }
    });
    connect(uiSet->ok,&QPushButton::clicked,this,[=](){
        QVector<std::tuple<QString,QString>> data;
        for(int i=0;i<uiSet->info->rowCount();i++){
            std::tuple<QString,QString> t;
            if(uiSet->info->item(i,0)!=NULL && uiSet->info->item(i,1)!=NULL && uiSet->info->item(i,2)!=NULL &&\
                uiSet->info->item(i,0)->text()!="" && uiSet->info->item(i,1)->text()!="" && uiSet->info->item(i,2)->text()!=""){
                t = std::make_tuple(uiSet->info->item(i,1)->text(),uiSet->info->item(i,2)->text());
            }
            data.append(t);
        }
        DataBase::instance()->insertclassScheduleInfo(account,data);
        emit entrySuccess();
    });
}

Ui::setTime* setTime::getUi(){
    return uiSet;
}

setTime::~setTime(){
    delete uiSet;
}

faceRecongnition::faceRecongnition(QString account,QWidget *parent): QWidget(parent), ui(new Ui::faceRecongnition){
    ui->setupUi(this);

    //初始化ui样式
    this->setUiStyle();
    //初始化部分值
    this->initial();
    //安装事件过滤器
    ui->max->installEventFilter(this);

    //关闭按钮功能
    connect(ui->close,&QPushButton::clicked,this,[=](){
        widgetOut(this);
        QTimer::singleShot(325,this,[=](){
            this->close();
        });
    });

    //最小化按钮功能
    connect(ui->min,&QPushButton::clicked,this,[=](){
        this->showMinimized();
    });

    //注册通信类型
    qRegisterMetaType<cv::Mat>("cv::Mat");
    qRegisterMetaType<cv::Ptr<cv::face::FaceRecognizer>>("cv::Ptr<cv::face::FaceRecognizer>");
    qRegisterNormalizedMetaType<cv::CascadeClassifier>("cv::CascadeClassifier");


    this->threadfacerecord->moveToThread(&thread_faceRecord);
    this->thread_faceRecord.start();

    connect(this,&faceRecongnition::startFaceRecord,threadfacerecord,&threadFaceRecord::faceRecord);
    connect(threadfacerecord,&threadFaceRecord::imageToRecord,this,[=](QImage image){
        ui->viewFaceImage->setPixmap(QPixmap::fromImage(image));
    });
    //录入人脸功能
    connect(ui->recordFace,&QPushButton::clicked,this,[=](){
        informationEntry->show();
        disconnect(informationEntry,&InformationEntry::entrySuccess,nullptr,nullptr);
        connect(informationEntry,&InformationEntry::entrySuccess,this,[=](){
            emit startFaceRecord(informationEntry->getUi()->name->text(),informationEntry->getUi()->gender->text()\
                                 ,informationEntry->getUi()->stuNumber->text(),informationEntry->getUi()->age->text()\
                                 ,informationEntry->getUi()->classes->text());
            informationEntry->hide();
        });
    });

    //管理信息界面
    connect(ui->management,&QPushButton::clicked,this,[=](){
        if(manage == nullptr){
            manage = new managementInfo(account);
        }
        disconnect(manage,&managementInfo::returnWidget,nullptr,nullptr);
        connect(manage,&managementInfo::returnWidget,this,[=](){
            this->show();
        });
        widgetOut(this);
        QTimer::singleShot(325,this,[=](){
            this->hide();
            manage->show();
        });
    });

    //关闭摄像头
    connect(ui->closeCap,&QPushButton::clicked,this,[=](){
        this->threadfacerecord->stopFaceRecord();
        ui->viewFaceImage->setPixmap(QPixmap());
        QTimer::singleShot(200,this,[=](){
            ui->viewFaceImage->setPixmap(QPixmap(":/faceImage.jpg"));
        });
    });

    //录入完成，关闭摄像头
    connect(threadfacerecord,&threadFaceRecord::end,this,[=](){
        emit ui->closeCap->clicked();
    });

    //通知主界面重新加载人脸识别模型
    connect(threadfacerecord,&threadFaceRecord::reload,this,[=](){
        emit reload();
    });

    //返回
    connect(ui->returnWidget,&QPushButton::clicked,this,[=](){
        this->threadfacerecord->stopFaceRecord();
        widgetOut(this);
        QTimer::singleShot(325,this,[=](){
            this->hide();
            emit returnWidget();
        });
    });
    //设置考勤时间
    connect(ui->setTime,&QPushButton::clicked,this,[=](){
        if(settime == nullptr){
            settime = new setTime(account);
        }
        settime->show();
        connect(settime,&setTime::entrySuccess,this,[=](){
            emit reSetTime(account);
            delete settime;
            settime = nullptr;
        });
    });
    //删除用户
    connect(ui->remove,&QPushButton::clicked,this,[=](){
        bool ok = false;
        QString stuNumber = QInputDialog::getText(this,"删除用户","输入学号",QLineEdit::Normal,"",&ok);
        if(ok && !stuNumber.isEmpty()){
            QMessageBox::information(nullptr,"删除用户","删除用户"+DataBase::instance()->getName(stuNumber)+"成功!");
            DeleteFileOrFolder(QString("video/"+DataBase::instance()->getName(stuNumber)+"_"+stuNumber));
            DataBase::instance()->deleteUserFromStuInfo(stuNumber);
        }
    });
}

void faceRecongnition::showEvent(QShowEvent *event){
    QPropertyAnimation *animation= new QPropertyAnimation(this,"geometry");
    //QScreen* screen = QGuiApplication::primaryScreen();
    animation->setDuration(320);
    //animation->setStartValue(QRect(this->x()+screen->geometry().width()/3, this->y()+screen->geometry().height()/4, this->width(),0));
    //animation->setEndValue(QRect(this->x()+screen->geometry().width()/3, this->y()+screen->geometry().height()/4, this->width(),this->height()));
    animation->setStartValue(QRect(this->x(), this->y(), this->initialWidth,0));
    animation->setEndValue(QRect(this->x() ,this->y(), this->initialWidth,this->initialHeight));
    animation->setEasingCurve(QEasingCurve::Linear);
    animation->start(QAbstractAnimation::DeleteWhenStopped);

    QWidget::showEvent(event);
}

void faceRecongnition::widgetOut(QWidget*target){
    QPropertyAnimation * animation = new QPropertyAnimation(target , "geometry");
    animation->setDuration(320);
    animation->setStartValue(QRect(target->x(),target->y(),target->width(),target->height()));
    animation->setEndValue(QRect(target->x(),target->y(),target->width(),0));
    animation->setEasingCurve(QEasingCurve::Linear);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void faceRecongnition::setUiStyle(){

    //设置窗口无边框
    setWindowFlags(Qt::Widget | Qt::FramelessWindowHint);
    //设置窗体透明,防止边距显示不同的颜色
    this->setAttribute(Qt::WA_TranslucentBackground, true);
    //设置标题栏颜色
    ui->widget_title->setStyleSheet(QString("QWidget#widget_title{background-color:#ECF5F9;border-radius:9px 9px}"));

    //设置按键区背景色
    ui->button_widget->setStyleSheet(QString("QWidget#button_widget{background-image:url(:/980.jpg)}"));

    //ui->viewFaceImage->setPixmap(QPixmap(":/faceRecongnition.png"));
    //ui->viewFaceImage->setScaledContents(true);
    ui->title->setText("人脸识别考勤管理系统");
    ui->icon->setPixmap(QPixmap(":/faceRecongnition.png"));
    ui->icon->setScaledContents(true);
    //设置图标
    ui->close->setIcon(QIcon(":/close.png"));
    ui->max->setIcon(QIcon(":/max.png"));
    ui->min->setIcon(QIcon(":/min.png"));
    ui->close->setStyleSheet(QString("QPushButton#close{border:none;background-color:transparent;}\
                                      QPushButton#close:hover{background-color:red;border-radius:3px 3px;}"));
    ui->close->setIcon(QIcon(":/close.png"));
    ui->min->setStyleSheet(QString("QPushButton#min{border:none;background-color:transparent;}\
                                    QPushButton#min:hover{background-color:#C0C0C0;border-radius:3px 3px;}"));
    ui->min->setIcon(QIcon(":/min.png"));
    ui->max->setStyleSheet(QString("QPushButton#max{border:none;background-color:transparent;}\
                                    QPushButton#max:hover{background-color:#C0C0C0;border-radius:3px 3px;}"));
    ui->max->setIcon(QIcon(":/max.png"));
    //设置边框阴影
    ui->widget->setStyleSheet(QString("QWidget#widget{background-color:rgb(243,243,243);border-radius:9px 9px;}"));
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setOffset(0, 0);
    shadow->setColor(Qt::gray);
    shadow->setBlurRadius(15);
    ui->widget->setGraphicsEffect(shadow);

    //圆角
    ui->viewFaceImage->setStyleSheet(QString("QLabel#viewFaceImage{border-radius:7px 7px}"));
    //设置人脸识别默认图片
    ui->viewFaceImage->setPixmap(QPixmap(":/faceImage.jpg"));
    ui->viewFaceImage->setScaledContents(true);
}

//实现窗口拖动
void faceRecongnition::mousePressEvent(QMouseEvent* event){
    if (event->button() == Qt::LeftButton) {
        m_dragStartPosition = event->pos();
        m_isDragging = true;
    }
    QWidget::mousePressEvent(event);
}
void faceRecongnition::mouseMoveEvent(QMouseEvent* event){
    if (m_isDragging) {
        QPoint diff = event->pos() - m_dragStartPosition;
        QPoint newTopLeft = this->geometry().topLeft() + diff;
        move(newTopLeft);
    }
    QWidget::mouseMoveEvent(event);
}
void faceRecongnition::mouseReleaseEvent(QMouseEvent* event){
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
    }
    QWidget::mouseReleaseEvent(event);
}

void faceRecongnition::initial(){
    this->initialWidth = this->width();
    this->initialHeight = this->height();
    this->threadfacerecord = new threadFaceRecord();
    this->informationEntry = new InformationEntry();
}

//实现最大化按钮图标以及窗口大小变化
bool faceRecongnition::eventFilter(QObject *watched, QEvent *event){
    if(watched == ui->max && event->type() == QEvent::MouseButtonPress){
        QMouseEvent* mouseEvent = dynamic_cast<QMouseEvent*>(event);
        if(mouseEvent->button() == Qt::LeftButton){
            if(widgetMax){
                this->setGeometry(initX,initY,initSize.width(),initSize.height());
                this->move(initX,initY);
                ui->max->setIcon(QIcon(":/max.png"));
                widgetMax = false;
                return QWidget::eventFilter(watched,event);
            }else{
                this->showMaximized();
                ui->max->setIcon(QIcon(":/max1.png"));
                widgetMax = true;
                return QWidget::eventFilter(watched,event);
            }
        }
    }
    return QWidget::eventFilter(watched,event);
}

bool faceRecongnition::DeleteFileOrFolder(const QString &strPath)//要删除的文件夹或文件的路径
{
    qDebug()<<strPath;
    if (strPath.isEmpty() || !QDir().exists(strPath))//是否传入了空的路径||路径是否存在
        return false;

    QFileInfo FileInfo(strPath);

    if (FileInfo.isFile())
        QFile::remove(strPath);
    else if (FileInfo.isDir()){
        QDir qDir(strPath);
        qDir.removeRecursively();
    }
    return true;
}

faceRecongnition::~faceRecongnition()
{
    delete ui;
    this->threadfacerecord->stopFaceRecord();
    this->thread_faceRecord.exit(0);
    this->thread_faceRecord.wait();
    delete this->threadfacerecord;
}
