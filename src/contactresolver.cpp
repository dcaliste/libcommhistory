/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2014 Jolla Ltd.
** Contact: John Brooks <john.brooks@jollamobile.com>
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

#include "contactresolver.h"

#include <QElapsedTimer>

#include "recipient_p.h"
#include "debug.h"

#include <seasidecache.h>

using namespace CommHistory;

namespace CommHistory {

class ContactResolverPrivate : public QObject, public SeasideCache::ResolveListener
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(ContactResolver)

public:
    ContactResolver *q_ptr;
    QSet<Recipient> pending;
    bool resolving;
    bool forceResolving;

    explicit ContactResolverPrivate(ContactResolver *parent);
    ~ContactResolverPrivate();

    void resolve(Recipient recipient);
    void checkIfFinishedAsynchronously();
    virtual void addressResolved(const QString &first, const QString &second, SeasideCache::CacheItem *item);

public slots:
    bool checkIfFinished();
};

} // namespace CommHistory

ContactResolver::ContactResolver(QObject *parent)
    : QObject(parent), d_ptr(new ContactResolverPrivate(this))
{
}

ContactResolverPrivate::ContactResolverPrivate(ContactResolver *parent)
    : QObject(parent), q_ptr(parent), resolving(false), forceResolving(false)
{
}

ContactResolverPrivate::~ContactResolverPrivate()
{
    SeasideCache::unregisterResolveListener(this);
}

bool ContactResolver::isResolving() const
{
    Q_D(const ContactResolver);
    return d->resolving;
}

bool ContactResolver::forceResolving() const
{
    Q_D(const ContactResolver);
    return d->forceResolving;
}

void ContactResolver::setForceResolving(bool enabled)
{
    Q_D(ContactResolver);
    d->forceResolving = enabled;
}

void ContactResolver::add(const Recipient &recipient)
{
    Q_D(ContactResolver);
    d->resolve(recipient);
    d->checkIfFinishedAsynchronously();
}

void ContactResolver::add(const RecipientList &recipients)
{
    Q_D(ContactResolver);

    foreach (const Recipient &recipient, recipients)
        d->resolve(recipient);

    d->checkIfFinishedAsynchronously();
}

void ContactResolver::add(const QList<Recipient> &recipients)
{
    Q_D(ContactResolver);

    foreach (const Recipient &recipient, recipients)
        d->resolve(recipient);

    d->checkIfFinishedAsynchronously();
}

void ContactResolverPrivate::resolve(Recipient recipient)
{
    if (!forceResolving && recipient.isContactResolved())
        return;

    Q_ASSERT(!recipient.localUid().isEmpty());
    if (recipient.localUid().isEmpty() || recipient.remoteUid().isEmpty()) {
        // Cannot match any contact. Set as resolved to nothing.
        RecipientPrivate::setResolved(&recipient, nullptr);
        return;
    }

    if (pending.contains(recipient))
        return;

    SeasideCache::CacheItem *item = 0;
    if (recipient.isPhoneNumber()) {
        item = SeasideCache::resolvePhoneNumber(this, recipient.remoteUid(), false);
    } else {
        item = SeasideCache::resolveOnlineAccount(this, recipient.localUid(), recipient.remoteUid(), false);
    }

    if (item) {
        RecipientPrivate::setResolved(&recipient, item);
    } else {
        pending.insert(recipient);
    }
}

void ContactResolverPrivate::checkIfFinishedAsynchronously()
{
    if (!resolving) {
        resolving = true;

        if (pending.isEmpty()) {
            bool ok = metaObject()->invokeMethod(this, "checkIfFinished", Qt::QueuedConnection);
            Q_UNUSED(ok);
            Q_ASSERT(ok);
        }
    }
}

bool ContactResolverPrivate::checkIfFinished()
{
    Q_Q(ContactResolver);
    if (resolving && pending.isEmpty()) {
        resolving = false;
        emit q->finished();
        return true;
    }
    return false;
}

void ContactResolverPrivate::addressResolved(const QString &first, const QString &second, SeasideCache::CacheItem *item)
{
    if (second.isEmpty()) {
        qWarning() << "Got addressResolved with empty UIDs" << first << second << item;
        return;
    } else if (first.isEmpty()) {
        // This resolution is for a phone number - we need to call back to libcontacts
        // to select the best match from multiple possible resolutions
        const Recipient::PhoneNumberMatchDetails phoneNumber(Recipient::phoneNumberMatchDetails(second));
        for (QSet<Recipient>::iterator it = pending.begin(); it != pending.end(); ) {
            if (it->matchesPhoneNumber(phoneNumber)) {
                // Look up the best match for the full number
                RecipientPrivate::setResolved(&(*it), SeasideCache::itemByPhoneNumber(it->remoteUid(), false));
                it = pending.erase(it);
            } else {
                ++it;
            }
        }
    } else {
        QSet<Recipient>::iterator it = pending.find(Recipient(first, second));
        if (it != pending.end()) {
            RecipientPrivate::setResolved(&(*it), item);
            pending.erase(it);
        }
    }

    checkIfFinished();
}

#include "contactresolver.moc"
