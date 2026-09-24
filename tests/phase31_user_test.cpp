#include "database/database_manager.h"
#include "repository/user_repository.h"
#include "service/user_service.h"
#include <QCoreApplication>
#include <QDebug>
#include <QSqlQuery>
#include <QTemporaryDir>
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir dir;ncs::DatabaseManager db(dir.filePath("p31.db"));if(!db.initialize())return 1;ncs::UserRepository repo(db);ncs::UserService service(db,repo);int pass=0,total=0;auto check=[&](bool ok,const char*m){++total;if(ok){++pass;qInfo()<<"PASS"<<m;}else qCritical()<<"FAIL"<<m;};auto created=service.registerUser("demo_user","secret1");check(created.success&&created.value.id>0,"register success");check(!service.registerUser("demo_user","secret1").success,"duplicate rejected");auto login=service.login("demo_user","secret1");check(login.success&&login.value.nickname=="demo_user","login success");check(!service.login("demo_user","wrongxx").success,"wrong password rejected");QSqlQuery q(db.connection());q.exec("SELECT password_hash,salt FROM user WHERE username='demo_user'");q.next();check(q.value(0).toString()!="secret1"&&q.value(0).toString().size()==64&&!q.value(1).toString().isEmpty(),"password hashed with salt");return pass==total?0:1;}
