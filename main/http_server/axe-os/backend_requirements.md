# 后端开发需求文档 (v1.2.0)

为了支持前端 v1.2.0 版本新增的 **安全认证 (Authentication & Security)** 功能，后端需要进行以下接口调整与功能实现。

## 1. 新增 API 接口

### 1.1 用户登录 (`POST /api/system/login`)
*   **用途**: 验证用户凭证并返回认证 Token。
*   **请求体 (Request Body)**:
    ```json
    {
        "username": "root",
        "password": "your_password"
    }
    ```
*   **响应 (成功 - 200)**:
    ```json
    {
        "token": "generated_session_token_string"
    }
    ```
*   **响应 (失败 - 401)**:
    ```json
    {
        "error": "Invalid credentials"
    }
    ```

## 2. 变更 API 接口

### 2.1 获取系统信息 (`GET /api/system/info`) `getMinerStatus`
*   **变更**: 响应数据中增加 `auth_enable` 和 `web_username` 字段。
*   **响应结构 (部分示例)**:
    ```json
    {
        "auth_enable": true,       // boolean: 当前是否开启了验证
        "web_username": "root",    // string: 当前用户名
        ...
    }
    ```

### 2.2 更新系统配置 (`PATCH /api/system`) `updateSystem`
*   **变更**: 支持处理新增的安全相关字段。
*   **扩展请求体**:
    ```json
    {
        "auth_enable": true,       // boolean: 开启/关闭 验证
        "web_username": "root",    // string: 新用户名
        "web_password": "new_pwd"  // string: 新密码 (仅在修改时发送)
    }
    ```
*   **处理逻辑**:
    *   如果 `auth_enable` 发生变化，更新全局状态。
    *   如果 `web_username` 或 `web_password` 字段存在且不为空，则更新 NVS/Config 中的对应配置。

## 3. 中间件 / 请求拦截 (Middleware)

### 3.1 权限检查 (Authentication Check)
*   **逻辑**: 对于**所有**修改状态的 API (POST, PATCH, PUT) 以及敏感数据的 GET API (如系统信息、日志)，必须检查请求头中是否包含有效的 Authorization 信息。
*   **Header 格式**: `Authorization: Bearer <token>`
*   **验证流程**:
    1.  检查系统配置 `auth_enable` 是否为 `true`。
    2.  如果开启验证，校验 Token 是否与当前活跃会话匹配且未过期。
    3.  如果无效或缺失，直接返回 `401 Unauthorized`。

## 4. 硬件/系统层建议 (Optional)
*   **Token 存储**: 需要一种机制来存储和管理 Session Token 的生命周期 (建议存储在内存中，并设置超时机制)。
*   **默认凭证**:
    *   用户名: `root`
    *   密码: `root` (或者每台设备唯一的默认密码)
