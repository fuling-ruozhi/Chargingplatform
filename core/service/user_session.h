#pragma once
#include "model/user.h"
namespace ncs { class UserSession{public:explicit UserSession(const User&u):user_(u){}const User&user()const{return user_;}qint64 userId()const{return user_.id;}void updateUser(const User&u){user_=u;}private:User user_;}; }
