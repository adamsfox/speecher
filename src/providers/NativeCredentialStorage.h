#pragma once

#include <QByteArray>
#include <QString>

namespace speecher {

QByteArray readNativeCredential(const QByteArray &service, const QByteArray &account, QString *error);
// Updates an existing entry. Never creates an entry after a concurrent logout.
bool writeNativeCredential(const QByteArray &service, const QByteArray &account,
                           const QByteArray &bytes, QString *error);

} // namespace speecher
