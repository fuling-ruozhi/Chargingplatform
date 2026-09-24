#pragma once
#include "business_error.h"
#include <QString>
namespace ncs { template<typename T>struct ServiceResult{bool success=false;BusinessErrorCode code=BusinessErrorCode::DatabaseError;QString message;T value;static ServiceResult ok(const T&v){return{true,BusinessErrorCode::Ok,QStringLiteral("ok"),v};}static ServiceResult fail(BusinessErrorCode c,const QString&m){return{false,c,m,T()};}}; }
