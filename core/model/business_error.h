#pragma once
namespace ncs {

enum class BusinessErrorCode {
    Ok = 0,
    AuthRequired = 3001,
    SessionExpired = 3002,
    Forbidden = 3003,
    InvalidArgument = 3901,
    UsernameExists = 3902,
    InvalidCredentials = 3903,
    DatabaseError = 6001,
    UserNotFound = 4005,
    InvalidPhone = 4001,
    InvalidVerificationCode = 4002,
    VerificationCodeExpired = 4003,
    UserFrozen = 4004,
    InvalidProfile = 4006,
    OtpCooldown = 4007,
    TooFrequent = 4008,
    InvalidRechargeAmount = 4101,
    InsufficientBalance = 4102,
    AdminInvalidCredentials = 4301,
    AdminLocked = 4302,
    StationNotFound = 3101,
    StationHasChargers = 3102,
    ChargerUnavailable = 3201,
    ActiveChargeNotFound = 3202,
    ChargerNotFound = 3203,
    ActiveOrderExists = 3204,
    ChargerFault = 3205,
    OrderNotFound = 3206,
    InvalidOrderOwner = 3207,
    InvalidOrderState = 3208,
    ReservationExpired = 3209,
    ConcurrentReservationConflict = 3210,
    ReviewAlreadyExists = 3301,
    InvalidReviewScore = 3302
};

}
