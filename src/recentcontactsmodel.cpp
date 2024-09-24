/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2013 Jolla Ltd. <matthew.vogt@jollamobile.com>
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

#include "recentcontactsmodel.h"

#include "databaseio_p.h"
#include "commhistorydatabase.h"
#include "eventmodel_p.h"
#include "contactlistener.h"

#include <seasidecache.h>

#include <QContactFavorite>

#include <QSqlQuery>
#include <QSqlError>

namespace CommHistory {

using namespace CommHistory;

static int eventContact(const Event &event)
{
    return event.recipients().contactIds().value(0);
}

static bool contactIsFavorite(int contactId)
{
    if (SeasideCache::CacheItem *item = SeasideCache::instance()->existingItem(static_cast<quint32>(contactId))) {
        QContactFavorite favoriteDetail = item->contact.detail<QContactFavorite>();
        return favoriteDetail.isFavorite();
    }
    return false;
}

class RecentContactsModelPrivate : public EventModelPrivate
{
public:
    Q_DECLARE_PUBLIC(RecentContactsModel)

    RecentContactsModelPrivate(EventModel *model)
        : EventModelPrivate(model),
          requiredProperty(RecentContactsModel::NoPropertyRequired),
          excludeFavorites(false),
          addressFlags(0)
    {
        setResolveContacts(EventModel::ResolveOnDemand);
    }

    virtual bool acceptsEvent(const Event &event) const;
    virtual bool fillModel(int start, int end, QList<Event> events, bool resolved);
    virtual void prependEvents(QList<Event> events, bool resolved);

    virtual void slotContactInfoChanged(const RecipientList &recipients);
    virtual void slotContactChanged(const RecipientList &recipients);
    virtual void slotContactDetailsChanged(const RecipientList &recipients);

private:
    void removeFavorites(const RecipientList &recipients);

    int requiredProperty;
    bool excludeFavorites;
    quint64 addressFlags;
    QList<Event> unresolvedEvents;
    QList<Event> resolvedEvents;
    QSet<int> resolvedContactIds;
};

bool RecentContactsModelPrivate::acceptsEvent(const Event &event) const
{
    // Contact must be resolved before we can do anything, so just accept
    Q_UNUSED(event);
    return true;
}

bool RecentContactsModelPrivate::fillModel(int start, int end, QList<Event> events, bool resolved)
{
    Q_UNUSED(start);
    Q_UNUSED(end);

    // This model doesn't fetchMore, so fill is only called once. We can use the prepend logic to get
    // the right contact behaviors.
    prependEvents(events, resolved);
    return true;
}

void RecentContactsModelPrivate::slotContactInfoChanged(const RecipientList &recipients)
{
    if (addressFlags != 0) {
        // Find if any of these recipients no longer matches our address requirements
        QSet<int> nonmatchingIds;
        foreach (const Recipient &recipient, recipients) {
            if (!recipient.matchesAddressFlags(addressFlags)) {
                nonmatchingIds.insert(recipient.contactId());
            }
        }

        if (!nonmatchingIds.isEmpty()) {
            // If any of our events no longer resolve to a contact, remove them
            int rowCount = eventRootItem->childCount();
            for (int row = 0; row < rowCount; ) {
                const Event &existing(eventRootItem->eventAt(row));
                const int contactId(eventContact(existing));
                if (nonmatchingIds.contains(contactId)) {
                    deleteFromModel(existing.id());
                    --rowCount;

                    nonmatchingIds.remove(contactId);
                    if (nonmatchingIds.isEmpty())
                        break;
                } else {
                    ++row;
                }
            }
        }
    }

    if (excludeFavorites) {
        // We may also need to update for favorite status changes
        removeFavorites(recipients);
    }

    EventModelPrivate::slotContactInfoChanged(recipients);
}

void RecentContactsModelPrivate::slotContactChanged(const RecipientList &recipients)
{
    // If any of our events no longer resolve to a contact, remove them
    int rowCount = eventRootItem->childCount();
    for (int row = 0; row < rowCount; ) {
        const Event &existing(eventRootItem->eventAt(row));
        if (existing.contactRecipients().isEmpty()) {
            deleteFromModel(existing.id());
            --rowCount;
        } else {
            ++row;
        }
    }

    EventModelPrivate::slotContactChanged(recipients);
}

void RecentContactsModelPrivate::slotContactDetailsChanged(const RecipientList &recipients)
{
    if (excludeFavorites) {
        // If any of these contacts have become favorites, they should be removed from our model
        removeFavorites(recipients);
    }

    EventModelPrivate::slotContactDetailsChanged(recipients);
}

void RecentContactsModelPrivate::removeFavorites(const RecipientList &recipients)
{
    QList<int> favoriteIds;
    foreach (const Recipient &recipient, recipients) {
        const int contactId(recipient.contactId());
        if (contactIsFavorite(contactId)) {
            favoriteIds.append(contactId);
        }
    }

    if (!favoriteIds.isEmpty()) {
        int rowCount = eventRootItem->childCount();
        for (int row = 0; row < rowCount; ) {
            const Event &existing(eventRootItem->eventAt(row));
            const int contactId(eventContact(existing));
            if (favoriteIds.contains(contactId)) {
                deleteFromModel(existing.id());
                --rowCount;
            } else {
                ++row;
            }
        }
    }
}

void RecentContactsModelPrivate::prependEvents(QList<Event> events, bool resolved)
{
    Q_Q(RecentContactsModel);

    QList<Event>::iterator it = events.begin(), end = events.end();
    for ( ; it != end; ++it) {
        Event &event(*it);
        if (eventCategoryMask == Event::AnyCategory || (event.category() & eventCategoryMask) != 0) {
            if (!resolved) {
                // Queue these events for resolution if required
                unresolvedEvents.append(event);
            } else {
                // Ensure the new events represent different contacts
                const Recipient &recipient = event.recipients().first();
                const int contactId = recipient.contactId();
                if (contactId != 0 && !resolvedContactIds.contains(contactId)) {
                    // If this contact is a favorite, then don't include the event in our results
                    if (excludeFavorites && contactIsFavorite(contactId)) {
                        continue;
                    }

                    // Is this contact relevant to our required types?
                    if (!addressFlags || recipient.matchesAddressFlags(addressFlags)) {
                        resolvedContactIds.insert(contactId);
                        resolvedEvents.append(event);

                        // Don't add any more events than we can present
                        if (resolvedEvents.count() == queryLimit) {
                            break;
                        }
                    }
                }
            }
        }
    }

    if (!unresolvedEvents.isEmpty()) {
        // Do we have enough items to reach the limit?
        if (queryLimit == 0 || resolvedEvents.count() < queryLimit) {
            resolveAddedEvents(QList<Event>() << unresolvedEvents.takeFirst());
            return;
        }

        // We won't ever show these events; just drop them
        unresolvedEvents.clear();
    }

    if (!resolvedEvents.isEmpty()) {
        // Does the new event replace an existing event?
        QSet<int> removeSet;
        const int rowCount = eventRootItem->childCount();
        for (int row = 0; row < rowCount; ++row) {
            const Event &existing(eventRootItem->eventAt(row));
            if (resolvedContactIds.contains(eventContact(existing))) {
                removeSet.insert(row);
            }
        }

        // Do we need to remove the final event(s) to maintain the limit?
        if (queryLimit) {
            int trimCount = rowCount + resolvedEvents.count() - removeSet.count() - queryLimit;
            int removeIndex = rowCount - 1;
            while (trimCount > 0) {
                while (removeSet.contains(removeIndex)) {
                    --removeIndex;
                }
                if (removeIndex < 0) {
                    break;
                } else {
                    removeSet.insert(removeIndex);
                    --removeIndex;
                    --trimCount;
                }
            }
        }

        // Remove the rows that have been made obsolete
        QList<int> removeIndices = removeSet.values();
        std::sort(removeIndices.begin(), removeIndices.end());

        int count;
        while ((count = removeIndices.count()) != 0) {
            int end = removeIndices.last();
            int consecutiveCount = 1;
            for ( ; (count - consecutiveCount) > 0; ++consecutiveCount) {
                if (removeIndices.at(count - 1 - consecutiveCount) != (end - consecutiveCount)) {
                    break;
                }
            }

            removeIndices = removeIndices.mid(0, count - consecutiveCount);

            int start = (end - consecutiveCount + 1);
            q->beginRemoveRows(QModelIndex(), start, end);
            while (end >= start) {
                eventRootItem->removeAt(end);
                --end;
            }
            q->endRemoveRows();
        }

        // Insert the new events at the start
        int start = 0;
        q->beginInsertRows(QModelIndex(), start, resolvedEvents.count() - 1);
        QList<Event>::const_iterator it = resolvedEvents.constBegin(), end = resolvedEvents.constEnd();
        for ( ; it != end; ++it) {
            eventRootItem->insertChildAt(start++, new EventTreeItem(*it, eventRootItem));
        }
        q->endInsertRows();

        resolvedEvents.clear();
        resolvedContactIds.clear();
    }

    if (resolved) {
        modelUpdatedSlot(true);
        emit q->resolvingChanged();
    }
}

RecentContactsModel::RecentContactsModel(QObject *parent)
    : EventModel(*new RecentContactsModelPrivate(this), parent)
{
}

RecentContactsModel::~RecentContactsModel()
{
}

int RecentContactsModel::requiredProperty() const
{
    Q_D(const RecentContactsModel);
    return d->requiredProperty;
}

void RecentContactsModel::setRequiredProperty(int requiredProperty)
{
    Q_D(RecentContactsModel);

    quint64 addressFlags = 0;
    if (requiredProperty & RecentContactsModel::PhoneNumberRequired)
        addressFlags |= QContactStatusFlags::HasPhoneNumber;
    if (requiredProperty & RecentContactsModel::EmailAddressRequired)
        addressFlags |= QContactStatusFlags::HasEmailAddress;
    if (requiredProperty & RecentContactsModel::AccountUriRequired)
        addressFlags |= QContactStatusFlags::HasOnlineAccount;

    d->addressFlags = addressFlags;
    if (d->requiredProperty != requiredProperty) {
        d->requiredProperty = requiredProperty;
        emit requiredPropertyChanged();
    }
}

bool RecentContactsModel::excludeFavorites() const
{
    Q_D(const RecentContactsModel);
    return d->excludeFavorites;
}

void RecentContactsModel::setExcludeFavorites(bool exclude)
{
    Q_D(RecentContactsModel);
    if (d->excludeFavorites != exclude) {
        d->excludeFavorites = exclude;
        emit excludeFavoritesChanged();
    }
}

bool RecentContactsModel::resolving() const
{
    Q_D(const RecentContactsModel);

    return !d->isReady || (d->addResolver && d->addResolver->isResolving()) ||
           (d->receiveResolver && d->receiveResolver->isResolving());
}

bool RecentContactsModel::getEvents()
{
    Q_D(RecentContactsModel);

    beginResetModel();
    d->clearEvents();
    endResetModel();

    QString categoryClause, limitClause;
    if (d->eventCategoryMask != Event::AnyCategory) {
        categoryClause = QStringLiteral("WHERE ") + DatabaseIOPrivate::categoryClause(d->eventCategoryMask);
    }
    if (d->queryLimit) {
        // Default to 4x the configured limit, because some of the addresses may
        // resolve to the same final contact, and others will match favorites
        limitClause = QStringLiteral("LIMIT ") + QString::number(4 * d->queryLimit);
    }

    QString q = DatabaseIOPrivate::eventQueryBase() + QString::fromLatin1(
" WHERE Events.id IN ("
  " SELECT lastId FROM ("
    " SELECT max(id) AS lastId, max(endTime) FROM Events"
    " JOIN ("
      " SELECT remoteUid, localUid, max(endTime) AS lastEventTime FROM Events"
      " %1"
      " GROUP BY remoteUid, localUid"
      " ORDER BY lastEventTime DESC"
      " %2"
    " ) AS LastEvent ON Events.endTime = LastEvent.lastEventTime"
                   " AND Events.remoteUid = LastEvent.remoteUid"
                   " AND Events.localUid = LastEvent.localUid"
    " GROUP BY Events.remoteUid, Events.localUid"
  " )"
" )"
" ORDER BY Events.endTime DESC").arg(categoryClause).arg(limitClause);

    QSqlQuery query = d->prepareQuery(q, 0, 0);

    bool re = d->executeQuery(query);
    if (re)
        emit resolvingChanged();
    return re;
}

} // namespace CommHistory
