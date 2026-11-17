#pragma once

#include <QByteArray>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QSysInfo>

#include <string>

// A stable identifier of this machine, the key for the HMAC of shortcuts.xml
inline std::string getMachineGUID()
{
	return QSysInfo::machineUniqueId().toStdString();
}

// Lowercase hexadecimal HMAC-SHA256
inline std::string computeHMAC(const std::string& key, const std::string& data)
{
	const QByteArray mac = QMessageAuthenticationCode::hash(QByteArray::fromStdString(data), QByteArray::fromStdString(key), QCryptographicHash::Sha256);
	return mac.toHex().toStdString();
}
