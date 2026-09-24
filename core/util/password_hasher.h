#pragma once
#include <QString>
namespace ncs { class PasswordHasher{public:static QString makeSalt();static QString hash(const QString&,const QString&);static bool verify(const QString&,const QString&,const QString&);}; }
