#pragma once
class QSqlDatabase; class QString;
namespace ncs { class SeedData { public: static bool populate(QSqlDatabase &, QString *); }; }
