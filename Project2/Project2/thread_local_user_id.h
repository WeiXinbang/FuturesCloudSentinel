#pragma once
// thread_local_user_id.h
#ifndef THREAD_LOCAL_USER_ID_H
#define THREAD_LOCAL_USER_ID_H

#define WIN32_LEAN_AND_MEAN
#include <cstdint>  // 用于 uint64_t

// 线程局部存储用户 ID（每个线程独立一份）
class ThreadLocalUserID {
public:
    // 设置当前线程的用户 ID
    static void SetUserID(uint64_t user_id) {
        s_user_id = user_id;
    }

    // 获取当前线程的用户 ID（默认返回 0 表示未设置）
    static uint64_t GetUserID() {
        return s_user_id;
    }

    // 清空当前线程的用户 ID（如用户退出登录）
    static void ClearUserID() {
        s_user_id = 0;
    }

private:
    // thread_local 关键字：每个线程有独立的 s_user_id 实例
    static thread_local uint64_t s_user_id;
};



#endif // THREAD_LOCAL_USER_ID_H