/*
 *  TcpSenderPix.h
 *
 *  A Simple TCP client send QPixma on demand
 *
 */
#ifndef TCPSENDPIX_H
#define TCPSENDPIX_H

#include <QString>
#include <QTcpSocket>
#include <QPixmap>
//#include <QTime>
#include <QTimer>
#include <QElapsedTimer>

class TcpSendPix : public QObject
{
Q_OBJECT

public:
	TcpSendPix(QString address, int port);
	void send(QPixmap pm);
	QElapsedTimer timeElapsed;
	QTimer timerSend;

private slots:
	void slotConnected();
	void slotTimeOut();

private:
	void SendStart(QPixmap pm);
	void openConnection();
	void closeConnection();

private:
	QPixmap pm;
	QString address;
	int port;
	QTcpSocket*      socket;
};

#endif // TCPSENDPIX_H
