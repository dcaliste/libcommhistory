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

#include "commonutils.h"
#include "commonutils_p.h"
#include "libcommhistoryexport.h"

#include <qtcontacts-extensions.h>
#include <qtcontacts-extensions_impl.h>

#include <QString>
#include <QSettings>

namespace {

int phoneNumberMatchLength()
{
    // TODO: use a configuration variable to make this configurable
    static int numberMatchLength = QtContactsSqliteExtensions::DefaultMaximumPhoneNumberCharacters;
    return numberMatchLength;
}

}

namespace CommHistory {
LIBCOMMHISTORY_EXPORT bool localUidComparesPhoneNumbers(const QString &localUid)
{
    return localUid.startsWith(RING_ACCOUNT);
}

LIBCOMMHISTORY_EXPORT QString normalizePhoneNumber(const QString &number, bool validate)
{
    // Validate the number, and retain the dial string
    QtContactsSqliteExtensions::NormalizePhoneNumberFlags flags(QtContactsSqliteExtensions::KeepPhoneNumberDialString);
    if (validate)
        flags |= QtContactsSqliteExtensions::ValidatePhoneNumber;

    return QtContactsSqliteExtensions::normalizePhoneNumber(number, flags);
}

LIBCOMMHISTORY_EXPORT QString minimizePhoneNumber(const QString &number)
{
    return QtContactsSqliteExtensions::minimizePhoneNumber(number, phoneNumberMatchLength());
}

}
