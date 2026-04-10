#include "qdatejalali.h"

QDATEJALALI::QDATEJALALI(QObject *parent) : QObject(parent)
{
    this->setParent(parent);
    shmdate.day = shmdate.month = 0; shmdate.year = 0 - 1300;
    mildate.day = mildate.month = 0;mildate.year = 0 - 2000;
}

bool QDATEJALALI::SetMiladiDate(int _year, int _month, int _day, int _dayOfWeek)
{
    mildate.day = _day;
    mildate.month = _month;
    mildate.year = _year - 2000;
    dayOfWeek = _dayOfWeek;
    miltoshmcv(mildate.year,mildate.month,mildate.day);
    return true;
}

bool QDATEJALALI::SetMiladiDate(QString Date)
{
    QStringList DatePart = Date.split("/");
    if(DatePart.count() == 3)
    {
        mildate.year =  QString(DatePart[0]).toInt() - 2000;
        mildate.month =  QString(DatePart[1]).toInt();
        mildate.day =  QString(DatePart[2]).toInt();
        miltoshmcv(mildate.year,mildate.month,mildate.day);
        return true;
    }
    else
    {
        mildate.day = mildate.month = 0;mildate.year = 0 - 2000;
        miltoshmcv(mildate.year,mildate.month,mildate.day);
        return false;
    }
}

bool QDATEJALALI::SetShamsiDate(int _year, int _month, int _day, int _dayOfWeek)
{
    shmdate.day = _day;
    shmdate.month = _month;
    shmdate.year = _year - 1300;
    dayOfWeek = _dayOfWeek;
    shmtomilcv(shmdate.year,shmdate.month,shmdate.day);
    return true;
}

bool QDATEJALALI::SetShamsiDate(QString Date)
{
    QStringList DatePart = Date.split("/");
    if(DatePart.count() == 3)
    {
        shmdate.year = DatePart[0].toInt() - 1300;
        shmdate.month = DatePart[1].toInt();
        shmdate.day = DatePart[2].toInt();
        shmtomilcv(shmdate.year,shmdate.month,shmdate.day);
        return true;
    }
    else
    {
        shmdate.day = shmdate.month = 0; shmdate.year = 0 - 1300;
        shmtomilcv(shmdate.year,shmdate.month,shmdate.day);
        return false;
    }
}

bool QDATEJALALI::ProcNow()
{
    QDateTime Now = QDateTime::currentDateTime();
    SetMiladiDate(Now.date().year(),Now.date().month(),Now.date().day(),Now.date().dayOfWeek());
    return true;
}

int QDATEJALALI::getdaymi()
{
    return mildate.day;
}

int QDATEJALALI::getmonthmi()
{
    return mildate.month;
}

QString QDATEJALALI::getmonthmistring()
{
    switch (mildate.month) {
    case 1:
        return QString("January");
    case 2:
        return QString("February");
    case 3:
        return QString("March");
    case 4:
        return QString("April");
    case 5:
        return QString("May");
    case 6:
        return QString("June");
    case 7:
        return QString("July");
    case 8:
        return QString("August");
    case 9:
        return QString("September");
    case 10:
        return QString("October");
    case 11:
        return QString("November");
    case 12:
        return QString("December");
    default:
        return QString("Not in month range");
    }
}

int QDATEJALALI::getyearmi()
{
    return mildate.year + 2000;
}

QString QDATEJALALI::getdayofweekmi()
{
    switch (dayOfWeek) {
    case 7:
        return QString("sunday");
    case 1:
        return QString("monday");
    case 2:
        return QString("tuesday");
    case 3:
        return QString("wednesday");
    case 4:
        return QString("thursday");
    case 5:
        return QString("friday");
    case 6:
        return QString("saturday");
    default:
        return QString("Error");
    }
}

int QDATEJALALI::getdaysh()
{
    return shmdate.day;
}

int QDATEJALALI::getmonthsh()
{
    return shmdate.month;
}

QString QDATEJALALI::getmonthshstring()
{
    switch (shmdate.month) {
    case 1:
        return QString("فروردین");
    case 2:
        return QString("اردیبهشت");
    case 3:
        return QString("خرداد");
    case 4:
        return QString("تیر");
    case 5:
        return QString("مرداد");
    case 6:
        return QString("شهریور");
    case 7:
        return QString("مهر");
    case 8:
        return QString("آبان");
    case 9:
        return QString("آذر");
    case 10:
        return QString("دی");
    case 11:
        return QString("بهمن");
    case 12:
        return QString("اسفند");
    default:
        return QString("خارج از بازه ی ماه ها");
    }
}

int QDATEJALALI::getyearsh()
{
    return shmdate.year + 1300;
}

QString QDATEJALALI::getdayofweeksh()
{
    switch (dayOfWeek) {
    case 7:
        return QString("یک شنبه");
    case 1:
        return QString("دو شنبه");
    case 2:
        return QString("سه شنبه");
    case 3:
        return QString("چهار شنبه");
    case 4:
        return QString("پنج شنبه");
    case 5:
        return QString("جمعه");
    case 6:
        return QString("شنبه");
    default:
        return QString("Error %1").arg(dayOfWeek);
    }
}

/*************************************************************************/
void QDATEJALALI::miltoshmcv(unsigned char ym,unsigned char mm,unsigned char dm)
{
    //ym -= 2000;
    unsigned char k,t1,t2;
    k=ym%4;
    if(k==3)
       k=2;
    k*=2;
    t1=miltable[k][mm-1];
    t2=miltable[k+1][mm-1];
    if(mm<3 || (mm==3 && dm<=miltable[k][mm-1]))
       shmdate.year = ym + 78;
    else
       shmdate.year = ym + 79;


    if(dm<=t1)
    {
       shmdate.day=dm+t2;
       shmdate.month=(mm+8)%12+1;
    }
    else
    {
       shmdate.day=dm-t1;
       shmdate.month=(mm+9)%12+1;
    }
}
/**********************************************************************/
void QDATEJALALI::shmtomilcv(unsigned char ys ,unsigned char ms,unsigned char ds)
{
    //ys -= 1300;
    unsigned char k,t1,t2;
    k = ys%4;
    if( k == 0)
       k = 2;
    else
       k = k + k;
    t1 =shmtable[k - 2][ms-1];
    t2 = shmtable[k-1][ms-1];
    if(ms<10 || (ms==10 && ds <= shmtable[k-2][ms-1]))
       mildate.year = ys - 79;
    else
       mildate.year = ys - 78;

    //mildate.year += 2000;

    if(ds <= t1)
    {
       mildate.day = ds + t2;
       mildate.month = (ms + 1)%12 + 1;
    }
    else
    {
       mildate.day= ds - t1;
       mildate.month= (ms + 2)%12 + 1;
    }
}
