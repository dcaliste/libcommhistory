/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Reto Zingg <reto.zingg@nokia.com>
**
** This library is free software; you can redistribute it and/or modify it
** under the terms of the GNU Lesser General Public License version 2.1 as
** published by the Free Software Foundation.
**
** This library is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
** or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
** License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this library; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
**
******************************************************************************/

#ifndef COMMHISTORY_ADAPTOR_H
#define COMMHISTORY_ADAPTOR_H

#include <QtDBus/QtDBus>
#include "event.h"
#include "group.h"
#include "libcommhistoryexport.h"

namespace CommHistory {

class LIBCOMMHISTORY_EXPORT Adaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT

    Q_CLASSINFO("D-Bus Interface", "com.nokia.commhistory")

public:
    Adaptor(QObject *parent = 0);

Q_SIGNALS:
    void eventsAdded(const QList<CommHistory::Event> &events);
    void eventsUpdated(const QList<CommHistory::Event> &events);
    void eventDeleted(int id);
    void groupsAdded(const QList<CommHistory::Group> &groups);
    void groupsUpdated(const QList<int> &groupIds);
    void groupsUpdatedFull(const QList<CommHistory::Group> &groups);
    void groupsDeleted(const QList<int> &groupIds);
};

}

#endif
