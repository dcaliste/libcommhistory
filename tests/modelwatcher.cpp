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

#include <QtTest/QtTest>

#include "modelwatcher.h"

ModelWatcher::ModelWatcher(QObject *parent)
    : UpdatesListener(QString(), parent),
      m_model(0),
      m_minCommitCount(0),
      m_minAddCount(0),
      m_committedCount(0),
      m_addedCount(0),
      m_updatedCount(0),
      m_deletedCount(0),
      m_lastDeleted(0),
      m_eventsCommitted(false),
      m_dbusSignalReceived(false)
{
    connect(this, &UpdatesListener::eventsAdded,
            this, &ModelWatcher::eventsAddedSlot);
    connect(this, &UpdatesListener::eventsUpdated,
            this, &ModelWatcher::eventsUpdatedSlot);
    connect(this, &UpdatesListener::eventDeleted,
            this, &ModelWatcher::eventDeletedSlot);
}

ModelWatcher::~ModelWatcher()
{
}

void ModelWatcher::setModel(CommHistory::EventModel *model)
{
    m_model = model;
    connect(m_model, SIGNAL(eventsCommitted(const QList<CommHistory::Event>&, bool)),
            this, SLOT(eventsCommittedSlot(const QList<CommHistory::Event>&, bool)));

    reset();
}

void ModelWatcher::reset()
{
    m_addedCount = 0;
    m_updatedCount = 0;
    m_deletedCount = 0;
    m_committedCount = 0;

    m_eventsCommitted = false;
    m_dbusSignalReceived = false;
    m_success = true;
}

bool ModelWatcher::isFinished() const
{
    if (m_minCommitCount >= 0 && (!m_committedCount || m_committedCount < m_minCommitCount))
        return false;

    if (!m_dbusSignalReceived)
        return false;

    if (m_addedCount < m_minAddCount || m_deletedCount < m_minDeleteCount)
        return false;

    return true;
}

#define TRY_COUNT(value, expected) \
    if (value < expected) { \
        QTest::qWait(0); \
        for (int i = 0; i < 5000 && value < expected; i += 50) \
            QTest::qWait(50); \
    } \
    if (value != expected) \
        qWarning() << Q_FUNC_INFO << "Incorrect signal count. Expected:" << expected << "Actual:" << value; \
    re &= value == expected

bool ModelWatcher::waitForCommitted(int count)
{
    bool re = true;
    TRY_COUNT(m_committedCount, count);
    reset();
    return re;
}

bool ModelWatcher::waitForAdded(int count, int committed)
{
    if (committed < 0)
        committed = count;

    bool re = true;
    TRY_COUNT(m_addedCount, count);
    TRY_COUNT(m_committedCount, committed);
    reset();
    return re;
}

bool ModelWatcher::waitForUpdated(int count)
{
    bool re = true;
    TRY_COUNT(m_updatedCount, count);
    TRY_COUNT(m_committedCount, count);
    reset();
    return re;
}

bool ModelWatcher::waitForDeleted(int count)
{
    bool re = true;
    TRY_COUNT(m_deletedCount, count);
    reset();
    return re;
}

#if 0
void ModelWatcher::waitForSignals(int minCommitted, int minAdded, int minDeleted)
{
    m_minCommitCount = minCommitted;
    m_minAddCount = minAdded;
    m_minDeleteCount = minDeleted;

    // Could this be implemented with a QTRY_VERIFY?
    QTRY_VERIFY(!m_success || isFinished());
    reset();
}
#endif

void ModelWatcher::eventsCommittedSlot(const QList<CommHistory::Event> &events,
                                       bool successful)
{
    // qDebug() << events.count() << "events, successful:" << successful;

    m_committedCount += successful ? events.size() : 0;
    m_success = successful;
}

void ModelWatcher::eventsAddedSlot(const QList<CommHistory::Event> &events)
{
    // qDebug() << events.count() << "events";
    m_addedCount += events.count();
    m_lastAdded = events;
    m_dbusSignalReceived = true;
}

void ModelWatcher::eventsUpdatedSlot(const QList<CommHistory::Event> &events)
{
    // qDebug() << events.count() << "events";
    m_updatedCount += events.count();
    m_lastUpdated = events;
    m_dbusSignalReceived = true;
}

void ModelWatcher::eventDeletedSlot(int id)
{
    // qDebug() << "deleted event#" << id;
    m_deletedCount++;
    m_lastDeleted = id;
}
