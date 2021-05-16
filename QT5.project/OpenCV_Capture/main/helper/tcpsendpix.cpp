/*
 *  TcpSenderPix.cpp
 *
 *  A SINGLE User TCP client/server w/ QPixmap
 *
 */
#include "tcpsendpix.h"

#include <QBuffer>

//TcpSenPix
//
TcpSendPix::TcpSendPix(QString address, int port)
{
    this->address = address;
    this->port = port;

    socket = new QTcpSocket(this);
    connect(socket, SIGNAL(connected()), this, SLOT(slotConnected()));

    connect(&timerSend, SIGNAL(timeout()), this,  SLOT(slotTimeOut()));
    timerSend.start(1000); //error if task took more than 1 sec
}

void TcpSendPix::slotConnected()
{
    timeElapsed.start();
    qDebug() << "TcpSend> Connected!" << timeElapsed.elapsed();
    SendStart(this->pm);
}

void TcpSendPix::openConnection()
{
    if (socket->state() == QAbstractSocket::UnconnectedState) {
        //qDebug() << "TcpSend> ...";
        if (socket->state() == QAbstractSocket::UnconnectedState) {
            socket->connectToHost(this->address, this->port, QIODevice::WriteOnly);
            qDebug() << "TcpSend> Connecting";
            }
        }
    else
       qDebug() << "TcpSend> Already connected!";
}

void TcpSendPix::send(QPixmap pm)
{
    this->pm = pm;
    openConnection();		// Start connection
}

void TcpSendPix::SendStart(QPixmap pm)
{
    if(socket->state() == QAbstractSocket::ConnectedState) {
        QByteArray ba;
        QBuffer buffer(&ba);
        pm.save(&buffer, "PNG");
        socket->write(ba);
        socket->flush();//Any pending ?
        qDebug() << "TcpSend>" << ba.size();
    } else {
        qDebug("Pixmap has not been send. No connection established!");
        }
    closeConnection();
}

void TcpSendPix::closeConnection()
{
    disconnect(&timerSend, SIGNAL(timeout()), this,  SLOT(slotTimeOut()));
    timerSend.stop();
    if (socket->state() == QAbstractSocket::ConnectedState) {
        socket->disconnectFromHost();
        qDebug() << "TcpSend> Closing" << timeElapsed.elapsed();
        }
    else
        qDebug() << "TcpSend> Disconnected!" << timeElapsed.elapsed() << "\n";
}

void TcpSendPix::slotTimeOut()
{
    qDebug() << "TcpSend> TimeOut!!!";
    closeConnection();
}
