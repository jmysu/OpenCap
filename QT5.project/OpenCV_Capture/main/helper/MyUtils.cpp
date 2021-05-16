
#include "MyUtils.h"
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>

QString MyUtils::stringMyFolder()
{
	QString pathDesktop = QStandardPaths::standardLocations(QStandardPaths::DesktopLocation)[0];
	QDir myDir(pathDesktop);
	myDir.mkpath("CaptureFolder");
	return myDir.absoluteFilePath("CaptureFolder");
}

QString MyUtils::stringMyFile(QString tm, QString type)
{
	return QString("%1/%2.%3").arg(MyUtils::stringMyFolder(), tm, type);
}

QString MyUtils::stringMyTime()
{
	QDateTime time = QDateTime::currentDateTime();
	//return time.toString("HH-mm-ss-zzz_dd-MM-yyyy");
	return time.toString("yyyyMMdd-HHmmss.zzz");
}

QString MyUtils::stringMyTitle()
{
	return QString("%1-%2   %3").arg(QCoreApplication::applicationName()).arg(APP_VERSION).arg(QSysInfo::prettyProductName());
}
