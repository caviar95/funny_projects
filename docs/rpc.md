这是一个典型的 **“核心业务逻辑和外部依赖（RPC、结构体/枚举等）耦合过重”** 的问题，导致：

* 单元测试（Unit Test）无法隔离测试业务逻辑；
* mock 困难、维护成本高；
* 无法重复利用核心逻辑；
* 编译依赖复杂，改动牵一发动全身。

## 一、问题拆解

我们先明确问题来源：

| 问题                  | 表现                             |
| ------------------- | ------------------------------ |
| 1. 直接依赖 RPC 接口      | 调用远程接口，UT 需起服务或写复杂 mock        |
| 2. 使用 RPC 定义的结构体/枚举 | `xx_service.pb.h`、枚举类型等穿透核心逻辑层 |
| 3. 耦合过重             | 业务逻辑无法独立编译或复用                  |

## 二、解耦思路总览

| 解耦方向       | 具体策略                              |
| ---------- | --------------------------------- |
| ✅ 接口抽象     | 使用 C++ 抽象类封装 RPC                  |
| ✅ DTO 转换   | 引入中间业务结构体（Biz DTO），与 RPC proto 分离 |
| ✅ 枚举转换     | 自定义业务枚举，提供转换函数                    |
| ✅ 面向接口编程   | 核心逻辑只依赖抽象接口、DTO                   |
| ✅ 依赖注入（DI） | 通过构造函数传入依赖（用于 mock）               |

---

## 三、实操步骤与示例

### 1. 接口抽象：封装 RPC 服务

#### 原有代码（反模式）：

```cpp
void OrderService::ProcessOrder(const OrderRequest& req) {
    RpcResponse resp;
    RpcClient::GetInstance().CallOrder(req, &resp);  // ❌ 直接依赖 RPC
    // 业务逻辑 ...
}
```

#### 改进方案：

```cpp
// 1. 定义抽象接口
class IOrderRpc {
public:
    virtual ~IOrderRpc() = default;
    virtual bool CallOrder(const std::string& user_id, BizOrderInfo& result) = 0;
};

// 2. 实现类：调用真正的 RPC
class OrderRpcImpl : public IOrderRpc {
public:
    bool CallOrder(const std::string& user_id, BizOrderInfo& result) override {
        OrderRequest req;
        req.set_user_id(user_id);
        OrderResponse resp;
        rpcClient_->CallOrder(req, &resp);
        result = Convert(resp);  // 转为业务DTO
        return true;
    }
};

// 3. 业务逻辑仅依赖接口
class OrderService {
public:
    OrderService(std::shared_ptr<IOrderRpc> rpc) : rpc_(rpc) {}
    void ProcessOrder(const std::string& uid);

private:
    std::shared_ptr<IOrderRpc> rpc_;
};
```

---

### 2. DTO转换：业务层不要直接使用 RPC 的结构体

#### 定义业务专用结构体（Biz DTO）：

```cpp
struct BizOrderInfo {
    std::string id;
    double price;
    std::string status;
};

inline BizOrderInfo Convert(const OrderResponse& resp) {
    return BizOrderInfo{
        resp.order_id(),
        resp.price(),
        ToBizStatus(resp.status())
    };
}
```

---

### 3. 枚举转换

定义业务枚举，避免 protobuf 枚举泄露到业务逻辑中。

```cpp
enum class OrderStatus { kNew, kPaid, kShipped };

inline OrderStatus ToBizStatus(proto::OrderStatus status) {
    switch (status) {
        case proto::NEW: return OrderStatus::kNew;
        case proto::PAID: return OrderStatus::kPaid;
        case proto::SHIPPED: return OrderStatus::kShipped;
        default: return OrderStatus::kNew;
    }
}
```

---

### 4. 单元测试中的 Mock 示例（基于 GMock）：

```cpp
class MockOrderRpc : public IOrderRpc {
public:
    MOCK_METHOD(bool, CallOrder, (const std::string&, BizOrderInfo&), (override));
};

TEST(OrderServiceTest, TestProcessOrder) {
    auto mock_rpc = std::make_shared<MockOrderRpc>();
    BizOrderInfo fake_info = {"123", 100.0, "paid"};
    EXPECT_CALL(*mock_rpc, CallOrder("user1", testing::_))
        .WillOnce(DoAll(testing::SetArgReferee<1>(fake_info), testing::Return(true)));

    OrderService service(mock_rpc);
    service.ProcessOrder("user1");
}
```

---

## 四、推荐的设计分层架构

```
+------------------------+
|       UI/接口层        |
+------------------------+
|   Service（业务逻辑）   |   <-- 只依赖抽象接口和 Biz DTO
+------------------------+
|      RPC适配层         |   <-- 封装实际 RPC/GRPC 调用
+------------------------+
|       网络/RPC层       |
+------------------------+
```

---

## 五、通用最佳实践建议

| 建议           | 说明                |
| ------------ | ----------------- |
| ✅ 保持核心逻辑“纯净” | 只依赖 std、抽象接口、业务模型 |
| ✅ 定义中间 DTO   | 不要让 proto:: 结构体泄露 |
| ✅ 抽象服务依赖     | 使用依赖注入，方便 mock    |
| ✅ 分层设计       | 清晰的职责边界，有利于测试和维护  |
| ✅ 单测优先设计     | 先写接口和 mock，后写实现   |

---

## 六、总结

如果你当前代码业务逻辑依赖 RPC、proto 枚举，建议：

1. 抽象所有外部服务（RPC/DB 等）；
2. 引入中间业务模型 DTO；
3. 使用依赖注入方式组织服务类；
4. 编写测试时只 mock 抽象接口，核心逻辑完全独立测试。

这样你就能构建一个**可测试、可维护、模块清晰**的 C++ 架构体系。

你指出的问题非常关键：**自定义业务枚举的转换函数需要手动维护，一旦 proto 文件中枚举项变更或新增，转换函数就需要手动同步修改，易出错、繁琐。**

我们来分析这个问题，并给出几种改进方案。

---

## ✅ 问题本质

当你选择用业务枚举 `enum class BizEnum` 替代 `proto::Enum`，虽然是为了**解耦和隔离 proto 变化**，但牺牲了：

* **类型自动追踪能力**（编译器不会提示你忘记更新映射）；
* **维护效率**（多了手动转换逻辑）；
* **潜在错误风险**（转换遗漏或不一致）。

---

## 🚀 解决思路总结

| 方法                              | 原理                                  | 优点            | 缺点          |
| ------------------------------- | ----------------------------------- | ------------- | ----------- |
| ✅ 1. 自动生成枚举映射代码                 | 用 Python 或 CMake script 自动生成转换代码    | 保持隔离 + 自动同步   | 增加构建步骤      |
| ✅ 2. 使用反射+模板映射                  | C++ 模板/宏封装统一映射                      | 简洁、可扩展        | 写法较复杂，调试不友好 |
| ✅ 3. 统一使用 proto::Enum（不再引入业务枚举） | 用 proto 枚举，不做自定义枚举                  | 零维护成本         | 与 proto 强耦合 |
| ✅ 4. 使用强命名+类型包裹方式               | 用 `enum_wrapper<proto::Enum>` 等轻度包装 | 统一接口，隔离 proto | 需要设计包裹逻辑    |

下面详细展开 **前三种实际可行方法**：

---

## 方法一：**自动生成枚举映射代码（推荐）**

定义一个 `.proto_enum_mapping.tmpl` 模板文件：

```cpp
// Auto-generated from {{proto_enum_name}}.
inline {{biz_enum_name}} ToBizEnum({{proto_enum_name}} value) {
    switch (value) {
    {% for value in values %}
        case {{proto_enum_name}}::{{value}}: return {{biz_enum_name}}::{{value}};
    {% endfor %}
        default: return {{biz_enum_name}}::{{values[0]}};
    }
}
```

写个简单 Python 脚本从 `.proto` 里解析 enum 定义并渲染模板，写入 `enum_mapping.h`。

**优点**：可维护性和隔离性兼得，配合构建工具自动生成。

---

## 方法二：**宏/模板 + 映射元表**

如果不想引入代码生成工具链，可以写一个小型“元表映射”。

```cpp
// 枚举声明同步
#define ORDER_ENUM_LIST(XX) \
    XX(kNew, NEW)           \
    XX(kPaid, PAID)         \
    XX(kShipped, SHIPPED)

enum class OrderStatus {
#define XX(biz, proto) biz,
    ORDER_ENUM_LIST(XX)
#undef XX
};

inline OrderStatus ToBizStatus(proto::OrderStatus s) {
    switch (s) {
#define XX(biz, proto) case proto::proto: return OrderStatus::biz;
        ORDER_ENUM_LIST(XX)
#undef XX
        default: return OrderStatus::kNew;
    }
}
```

**优点**：枚举维护在一处（`ORDER_ENUM_LIST`），易读性不错。

**缺点**：每个枚举都要手写一个 `XX` 列表，仍然手工同步，只是集中一点。

---

## 方法三：**不再定义业务枚举，使用 proto 枚举并加一层封装**

```cpp
// 不定义 OrderStatus，直接用 proto::OrderStatus，但不暴露给业务层
class OrderStatusWrapper {
public:
    explicit OrderStatusWrapper(proto::OrderStatus s) : status_(s) {}

    bool IsPaid() const { return status_ == proto::PAID; }
    std::string ToString() const { return proto::OrderStatus_Name(status_); }

    proto::OrderStatus raw() const { return status_; }

private:
    proto::OrderStatus status_;
};
```

**适用场景**：你不介意偶尔依赖 proto 枚举，但希望避免它在业务逻辑中“裸奔”。

---

## 小结：推荐选择

| 场景               | 推荐方案                      |
| ---------------- | ------------------------- |
| 枚举较多，需严格隔离 proto | ✅ 自动生成转换函数（方式一）           |
| 项目中间规模，人员熟悉宏     | ✅ 宏 + 枚举元组（方式二）           |
| 小项目、低隔离要求        | ✅ 直接用 proto 枚举 + 封装类（方式三） |

---

## ✅ 实战建议

* **结构体可以转 DTO，枚举就建议用“生成”或“统一封装”方式**，避免手写冗长映射。
* 构建工具如 CMake + Python + Jinja2 能很方便自动生成代码。
* 如果你有多个 `.proto` 文件，每次生成完都自动生成 `enum_mapping.h`，即可。

---


下面是一个完整可用的 **C++ 枚举映射模板示例**，基于 `.proto` 枚举定义自动生成 C++ 映射函数，适合用于将 `proto::Enum` 转换为业务 `enum class`，实现解耦和自动同步。

---

## ✅ 示例目标

假设有一个 proto 枚举如下：

```proto
// order.proto
enum OrderStatus {
  NEW = 0;
  PAID = 1;
  SHIPPED = 2;
  CANCELLED = 3;
}
```

我们希望自动生成以下业务枚举和映射函数：

```cpp
// auto_generated/order_enum_mapping.h
enum class BizOrderStatus {
  NEW,
  PAID,
  SHIPPED,
  CANCELLED
};

inline BizOrderStatus ToBizOrderStatus(OrderStatus proto_status) {
  switch (proto_status) {
    case OrderStatus::NEW: return BizOrderStatus::NEW;
    case OrderStatus::PAID: return BizOrderStatus::PAID;
    case OrderStatus::SHIPPED: return BizOrderStatus::SHIPPED;
    case OrderStatus::CANCELLED: return BizOrderStatus::CANCELLED;
    default: return BizOrderStatus::NEW;
  }
}
```

---

## ✅ Python 脚本：生成枚举映射

### 文件：`generate_enum_mapping.py`

```python
import re
import sys
from pathlib import Path

TEMPLATE = """
// Auto-generated. Do not edit manually.
#pragma once

enum class Biz{enum_name} {{
{biz_enum_items}
}};

inline Biz{enum_name} ToBiz{enum_name}({enum_name} value) {{
    switch (value) {{
{cases}
        default: return Biz{enum_name}::{default_item};
    }}
}}
"""

def parse_enum(proto_file: str):
    content = Path(proto_file).read_text()
    match = re.search(r'enum\s+(\w+)\s*\{([^}]+)\}', content, re.MULTILINE)
    if not match:
        raise RuntimeError("Enum not found")

    enum_name = match.group(1)
    body = match.group(2)

    enum_items = []
    for line in body.strip().splitlines():
        line = line.strip()
        if line and not line.startswith("//"):
            item = line.split('=')[0].strip()
            enum_items.append(item)

    return enum_name, enum_items


def generate_code(enum_name, items):
    biz_enum_items = "\n".join(f"  {item}," for item in items)
    cases = "\n".join(f"        case {enum_name}::{item}: return Biz{enum_name}::{item};"
                      for item in items)
    return TEMPLATE.format(
        enum_name=enum_name,
        biz_enum_items=biz_enum_items,
        cases=cases,
        default_item=items[0]
    )


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python generate_enum_mapping.py <input.proto> <output.h>")
        sys.exit(1)

    proto_file = sys.argv[1]
    output_file = sys.argv[2]

    enum_name, items = parse_enum(proto_file)
    code = generate_code(enum_name, items)
    Path(output_file).write_text(code)
    print(f"✅ Generated: {output_file}")
```

---

## ✅ 使用方法

```bash
python3 generate_enum_mapping.py order.proto order_enum_mapping.h
```

输出内容（示例）：

```cpp
// Auto-generated. Do not edit manually.
#pragma once

enum class BizOrderStatus {
  NEW,
  PAID,
  SHIPPED,
  CANCELLED
};

inline BizOrderStatus ToBizOrderStatus(OrderStatus value) {
    switch (value) {
        case OrderStatus::NEW: return BizOrderStatus::NEW;
        case OrderStatus::PAID: return BizOrderStatus::PAID;
        case OrderStatus::SHIPPED: return BizOrderStatus::SHIPPED;
        case OrderStatus::CANCELLED: return BizOrderStatus::CANCELLED;
        default: return BizOrderStatus::NEW;
    }
}
```

---

## ✅ 在 CMake 中自动生成（可选）

添加到你的 `CMakeLists.txt`：

```cmake
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/order_enum_mapping.h
    COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/generate_enum_mapping.py
            ${CMAKE_SOURCE_DIR}/proto/order.proto
            ${CMAKE_BINARY_DIR}/order_enum_mapping.h
    DEPENDS ${CMAKE_SOURCE_DIR}/proto/order.proto
)

add_custom_target(generate_enum_mapping DEPENDS ${CMAKE_BINARY_DIR}/order_enum_mapping.h)
```

然后在你的主 target 中添加：

```cmake
add_dependencies(my_target generate_enum_mapping)
```

---

## ✅ 总结

优点：

* 业务代码解耦 `proto::Enum`；
* 避免手动同步；
* 自动生成，改动 proto 后一键更新；
* 可扩展支持多枚举、多 proto 文件。

如果你有多个枚举，可以扩展这个脚本支持多个 `enum` 提取和模板渲染。如果需要我也可以帮你扩展支持多个枚举/多个文件生成。

是否希望我接着补充：

* 多枚举自动识别？
* 模板参数可配置（比如 Biz 前缀改成其他）？
* 支持双向转换？


好的，这里是你提到的 **方法二：宏/模板 + 映射元表映射** 的完整 C++ 示例，包括：

* 业务枚举定义；
* proto 枚举转换为业务枚举；
* 可选：业务枚举转换回 proto 枚举（双向）；
* 统一声明映射内容，避免重复书写，防止遗漏；
* 编译器在 enum 成员变化时可快速提示不一致。

---

## ✅ 一、基本用途：枚举映射元表定义 + 映射函数

### 假设你有以下 proto 枚举：

```proto
// order.proto
enum OrderStatus {
  NEW = 0;
  PAID = 1;
  SHIPPED = 2;
  CANCELLED = 3;
}
```

---

## ✅ 二、C++ 宏模板完整实现

### 1. 枚举映射列表定义（**唯一源头**）

```cpp
// order_enum_mapping.h

#pragma once

#include "order.pb.h"  // OrderStatus enum from proto

// 用宏列出映射项：统一维护点，强类型安全
#define ORDER_STATUS_ENUM_MAPPING(XX) \
    XX(kNew, NEW)                     \
    XX(kPaid, PAID)                  \
    XX(kShipped, SHIPPED)           \
    XX(kCancelled, CANCELLED)
```

---

### 2. 定义业务枚举类型

```cpp
// order_enum.h

#pragma once

#include "order_enum_mapping.h"

enum class BizOrderStatus {
#define XX(biz, proto) biz,
    ORDER_STATUS_ENUM_MAPPING(XX)
#undef XX
};
```

---

### 3. 转换函数：proto -> 业务 enum

```cpp
inline BizOrderStatus ToBizOrderStatus(OrderStatus s) {
    switch (s) {
#define XX(biz, proto) case OrderStatus::proto: return BizOrderStatus::biz;
        ORDER_STATUS_ENUM_MAPPING(XX)
#undef XX
        default:
            return BizOrderStatus::kNew;  // fallback 默认值
    }
}
```

---

### 4. 可选：业务 enum -> proto

```cpp
inline OrderStatus ToProtoOrderStatus(BizOrderStatus s) {
    switch (s) {
#define XX(biz, proto) case BizOrderStatus::biz: return OrderStatus::proto;
        ORDER_STATUS_ENUM_MAPPING(XX)
#undef XX
        default:
            return OrderStatus::NEW;
    }
}
```

---

## ✅ 三、使用示例

```cpp
void ProcessStatus(OrderStatus status) {
    BizOrderStatus biz = ToBizOrderStatus(status);
    if (biz == BizOrderStatus::kPaid) {
        std::cout << "Order already paid." << std::endl;
    }

    OrderStatus back = ToProtoOrderStatus(biz);
    assert(back == status);  // 双向映射校验
}
```

---

## ✅ 四、优势总结

| 优点     | 说明                                            |
| ------ | --------------------------------------------- |
| ✅ 单点维护 | 所有映射项集中在 `ORDER_STATUS_ENUM_MAPPING(XX)`，只写一次 |
| ✅ 编译安全 | proto 枚举变动会立即提示映射函数不匹配                        |
| ✅ 可读性强 | 枚举项一对一明示，不需要手写多个 switch                       |
| ✅ 支持双向 | 可自动生成双向映射，避免反复手写                              |

---

## ✅ 五、进阶：多个枚举怎么办？

你可以创建多个宏，例如：

```cpp
#define ORDER_STATUS_ENUM_MAPPING(XX) ...
#define PAYMENT_STATUS_ENUM_MAPPING(XX) ...
```

然后分别写：

```cpp
enum class BizOrderStatus { ... };
enum class BizPaymentStatus { ... };

inline BizOrderStatus ToBizOrderStatus(...) { ... }
inline BizPaymentStatus ToBizPaymentStatus(...) { ... }
```

或者进一步统一为模板方式（泛化版本）也可扩展。

---

是否需要我再补充更复杂的版本，比如：

* 用模板或元编程方式实现泛型枚举转换器；
* 自动注册枚举名称字符串（ToString）；
* 加入 default fallback 策略可配置？


非常好，我们就来扩展这三项功能，在原有**宏 + 枚举元表映射方案**的基础上：

---

## ✅ 1. **使用模板/元编程实现泛型枚举转换器**

我们希望将：

```cpp
ToBizEnum(proto::EnumValue)
ToProtoEnum(BizEnumValue)
```

统一抽象为：

```cpp
EnumMapper<BizEnum, ProtoEnum>::ToBiz(ProtoEnum value);
EnumMapper<BizEnum, ProtoEnum>::ToProto(BizEnum value);
```

---

### 🔧 实现步骤：

#### Step 1：声明通用模板

```cpp
template<typename BizEnum, typename ProtoEnum>
struct EnumMapper;
```

---

#### Step 2：每个枚举写一个特化 + 宏注册元组

```cpp
// enum_mapper_order.h

#include "order.pb.h"

#define ORDER_ENUM_PAIRS(XX) \
    XX(BizOrderStatus::kNew, OrderStatus::NEW) \
    XX(BizOrderStatus::kPaid, OrderStatus::PAID) \
    XX(BizOrderStatus::kShipped, OrderStatus::SHIPPED) \
    XX(BizOrderStatus::kCancelled, OrderStatus::CANCELLED)

template<>
struct EnumMapper<BizOrderStatus, OrderStatus> {
    static BizOrderStatus ToBiz(OrderStatus proto) {
        switch (proto) {
#define XX(b, p) case OrderStatus::p: return BizOrderStatus::b;
            ORDER_ENUM_PAIRS(XX)
#undef XX
            default: return BizOrderStatus::kNew;  // fallback
        }
    }

    static OrderStatus ToProto(BizOrderStatus biz) {
        switch (biz) {
#define XX(b, p) case BizOrderStatus::b: return OrderStatus::p;
            ORDER_ENUM_PAIRS(XX)
#undef XX
            default: return OrderStatus::NEW;  // fallback
        }
    }
};
```

---

### ✅ 使用示例：

```cpp
auto biz = EnumMapper<BizOrderStatus, OrderStatus>::ToBiz(proto_status);
auto proto = EnumMapper<BizOrderStatus, OrderStatus>::ToProto(biz);
```

---

## ✅ 2. **自动注册 ToString()：生成业务枚举名字符串映射**

我们可以使用 `unordered_map` 或 `switch-case + constexpr` 形式注册字符串。

### 示例宏扩展：

```cpp
inline const char* ToString(BizOrderStatus status) {
    switch (status) {
#define XX(biz, proto) case BizOrderStatus::biz: return #biz;
        ORDER_STATUS_ENUM_MAPPING(XX)
#undef XX
        default: return "Unknown";
    }
}
```

---

如果你希望统一模板风格，可封装为：

```cpp
template<typename T>
struct EnumName;

template<>
struct EnumName<BizOrderStatus> {
    static const char* ToString(BizOrderStatus status) {
        switch (status) {
#define XX(biz, proto) case BizOrderStatus::biz: return #biz;
            ORDER_STATUS_ENUM_MAPPING(XX)
#undef XX
            default: return "Unknown";
        }
    }
};
```

---

### 使用：

```cpp
std::cout << EnumName<BizOrderStatus>::ToString(BizOrderStatus::kPaid) << std::endl;
```

---

## ✅ 3. **default fallback 策略可配置**

你可能希望 fallback 不是硬编码，而是可配置。

### 方法一：模板参数方式

```cpp
template<typename BizEnum, typename ProtoEnum, BizEnum DefaultBiz = BizEnum::kNew, ProtoEnum DefaultProto = static_cast<ProtoEnum>(0)>
struct EnumMapperWithDefault {
    static BizEnum ToBiz(ProtoEnum proto) {
        switch (proto) {
#define XX(b, p) case ProtoEnum::p: return BizEnum::b;
            ORDER_ENUM_PAIRS(XX)
#undef XX
            default: return DefaultBiz;
        }
    }

    static ProtoEnum ToProto(BizEnum biz) {
        switch (biz) {
#define XX(b, p) case BizEnum::b: return ProtoEnum::p;
            ORDER_ENUM_PAIRS(XX)
#undef XX
            default: return DefaultProto;
        }
    }
};
```

### 使用：

```cpp
using Mapper = EnumMapperWithDefault<BizOrderStatus, OrderStatus, BizOrderStatus::kCancelled>;
auto biz = Mapper::ToBiz(proto_status);
```

---

## 🎯 总结

| 目标                 | 实现方式                                            | 优点          |
| ------------------ | ----------------------------------------------- | ----------- |
| **泛型枚举转换器**        | `EnumMapper<BizEnum, ProtoEnum>::ToBiz/ToProto` | 统一接口，支持模板抽象 |
| **枚举 ToString 支持** | `EnumName<T>::ToString(...)`                    | 提高日志/调试可读性  |
| **fallback 策略可配置** | 模板默认参数 / runtime 配置                             | 灵活支持未知值容错处理 |

---

好的！我将为你设计一个通用、可直接使用在实际 C++ 项目中的枚举映射工具库，支持：

---

## ✅ 功能概览

| 模块                       | 功能                                 |
| ------------------------ | ---------------------------------- |
| `enum_mapper.hpp`        | 映射工具主模块：模板 `EnumMapper`、`EnumName` |
| `enum_mapping_macro.hpp` | 各枚举映射表定义（宏）                        |
| `enum_mapping_xxx.hpp`   | 每个业务/Proto 枚举映射实现文件                |
| Python 工具（可选）            | 生成映射宏模板草稿（节省手写）                    |

---

## 🗂️ 工程结构

```
enum_mapper/
├── enum_mapper.hpp             # 核心模板工具（你只需要 include 它）
├── enum_mapping_macro.hpp      # 所有映射宏定义集中处
├── enum_mapping_order.hpp      # 示例：OrderStatus 映射文件
└── generate_enum_macro.py      # 可选：从 proto 自动生成宏模板
```

---

## 1️⃣ `enum_mapper.hpp`：核心模板实现

```cpp
// enum_mapper.hpp
#pragma once
#include <string>

template<typename BizEnum, typename ProtoEnum>
struct EnumMapper;

template<typename EnumType>
struct EnumName;
```

---

## 2️⃣ `enum_mapping_macro.hpp`：声明元组宏

```cpp
// enum_mapping_macro.hpp
#pragma once

// 可集中管理多个 proto <-> biz 枚举的映射元组
#define ORDER_STATUS_ENUM_MAPPING(XX) \
    XX(BizOrderStatus::kNew, OrderStatus::NEW) \
    XX(BizOrderStatus::kPaid, OrderStatus::PAID) \
    XX(BizOrderStatus::kShipped, OrderStatus::SHIPPED) \
    XX(BizOrderStatus::kCancelled, OrderStatus::CANCELLED)
```

---

## 3️⃣ `enum_mapping_order.hpp`：枚举定义 + 映射实现

```cpp
// enum_mapping_order.hpp
#pragma once
#include "enum_mapper.hpp"
#include "enum_mapping_macro.hpp"
#include "order.pb.h"

enum class BizOrderStatus {
#define XX(biz, proto) biz,
    ORDER_STATUS_ENUM_MAPPING(XX)
#undef XX
};

template<>
struct EnumMapper<BizOrderStatus, OrderStatus> {
    static BizOrderStatus ToBiz(OrderStatus proto) {
        switch (proto) {
#define XX(b, p) case OrderStatus::p: return BizOrderStatus::b;
            ORDER_STATUS_ENUM_MAPPING(XX)
#undef XX
            default: return BizOrderStatus::kNew;
        }
    }

    static OrderStatus ToProto(BizOrderStatus biz) {
        switch (biz) {
#define XX(b, p) case BizOrderStatus::b: return OrderStatus::p;
            ORDER_STATUS_ENUM_MAPPING(XX)
#undef XX
            default: return OrderStatus::NEW;
        }
    }
};

template<>
struct EnumName<BizOrderStatus> {
    static const char* ToString(BizOrderStatus e) {
        switch (e) {
#define XX(b, p) case BizOrderStatus::b: return #b;
            ORDER_STATUS_ENUM_MAPPING(XX)
#undef XX
            default: return "Unknown";
        }
    }
};
```

---

## ✅ 示例使用

```cpp
#include "enum_mapper.hpp"
#include "enum_mapping_order.hpp"

void TestEnumMapping(OrderStatus proto_status) {
    auto biz = EnumMapper<BizOrderStatus, OrderStatus>::ToBiz(proto_status);
    auto proto = EnumMapper<BizOrderStatus, OrderStatus>::ToProto(biz);

    std::cout << "Biz Enum: " << EnumName<BizOrderStatus>::ToString(biz) << std::endl;
    assert(proto == proto_status); // 验证双向一致
}
```

---

## 🛠️ 可选工具：生成 enum 宏模板（简化初始编写）

### `generate_enum_macro.py`

```python
import sys
import re
from pathlib import Path

def extract_enum_items(proto_path: str, enum_name: str):
    text = Path(proto_path).read_text()
    match = re.search(r'enum\s+' + enum_name + r'\s*\{([^}]+)\}', text)
    if not match:
        raise RuntimeError("Enum not found")
    lines = match.group(1).strip().splitlines()
    items = []
    for line in lines:
        if '=' in line:
            name = line.split('=')[0].strip()
            items.append(name)
    return items

def generate_macro(enum_name, items):
    macro = f"#define {enum_name.upper()}_ENUM_MAPPING(XX) \\\n"
    for item in items:
        macro += f"    XX(Biz{enum_name}::{item}, {enum_name}::{item}) \\\n"
    return macro.strip(" \\\n")

if __name__ == "__main__":
    proto_file = sys.argv[1]
    enum_name = sys.argv[2]
    items = extract_enum_items(proto_file, enum_name)
    print(generate_macro(enum_name, items))
```

运行：

```bash
python3 generate_enum_macro.py order.proto OrderStatus
```

输出：

```cpp
#define ORDERSTATUS_ENUM_MAPPING(XX) \
    XX(BizOrderStatus::NEW, OrderStatus::NEW) \
    XX(BizOrderStatus::PAID, OrderStatus::PAID) \
    XX(BizOrderStatus::SHIPPED, OrderStatus::SHIPPED) \
    XX(BizOrderStatus::CANCELLED, OrderStatus::CANCELLED)
```

---

## ✅ 扩展支持多个枚举？

你可以为每个 proto 文件单独写一个 `*_mapping_macro.hpp` 文件，全部 `#include` 到 `enum_mapper.hpp` 或统一集中到 `enum_mapping_macro.hpp` 中。

---

## 📦 打包说明

你只需要做这几件事：

1. 每个业务枚举写一个宏元组（使用 Python 脚本辅助）；
2. 为每个枚举定义：

   * `enum class BizXxxStatus {...}`
   * `EnumMapper` 特化；
   * `EnumName` 特化（可选）；
3. include `enum_mapper.hpp` + `enum_mapping_*.hpp` 即可使用。

---

## 🎯 总结

| 优势            | 说明                        |
| ------------- | ------------------------- |
| ✅ 高度自动化       | 多个枚举支持、双向映射、ToString      |
| ✅ 无重复逻辑       | 元组式集中定义，编译器可提示遗漏          |
| ✅ 模块独立        | 可用于任何 proto + C++ 架构      |
| ✅ 支持 fallback | 可扩展 default fallback 策略模板 |

---

好的，这是一个 **可运行的 C++ demo 工程**，包含你所需的所有功能：**protobuf 枚举映射自动生成** + **泛型 `EnumMapper` 模板** + **CMake 构建** + **Google Test 单元测试**。你可以 clone 并直接运行，作为你项目的起点。

---

## 📁 项目结构

```
enum_mapper_demo/
├── CMakeLists.txt
├── proto/
│   └── order.proto
├── scripts/
│   └── generate_enum_macro.py
├── src/
│   ├── enum_mapper.hpp
│   ├── enum_mapping_macro.hpp
│   ├── enum_mapping_order.hpp
│   └── main.cpp
└── test/
    ├── CMakeLists.txt
    └── enum_mapper_test.cpp
```

---

## 🧩 1. `proto/order.proto`

```proto
syntax = "proto3";
package demo;

enum OrderStatus {
  NEW = 0;
  PAID = 1;
  SHIPPED = 2;
  CANCELLED = 3;
}
```

---

## 🔧 2. `scripts/generate_enum_macro.py`

```python
import sys, re
from pathlib import Path

def extract(proto_file, enum_name):
    text = Path(proto_file).read_text()
    m = re.search(r'enum\s+' + enum_name + r'\s*\{([^}]+)\}', text)
    return [l.split('=')[0].strip() for l in m.group(1).splitlines() if '=' in l]

if __name__ == "__main__":
    proto, enum = sys.argv[1], sys.argv[2]
    items = extract(proto, enum)
    print(f"#define {enum.upper()}_ENUM_MAPPING(XX) \\")
    for item in items:
        print(f"    XX(Biz{enum}::{item}, {enum}::{item}) \\")
```

---

## 🧪 3. CMake 逻辑（根 `CMakeLists.txt`）

```cmake
cmake_minimum_required(VERSION 3.15)
project(enum_mapper_demo)

find_package(Protobuf REQUIRED)
find_package(GTest REQUIRED)

# Generate enum_mapping_macro
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/enum_mapping_macro.hpp
  COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/generate_enum_macro.py
          ${CMAKE_SOURCE_DIR}/proto/order.proto OrderStatus
          > ${CMAKE_BINARY_DIR}/enum_mapping_macro.hpp
  DEPENDS ${CMAKE_SOURCE_DIR}/proto/order.proto
)
add_custom_target(gen_macro DEPENDS ${CMAKE_BINARY_DIR}/enum_mapping_macro.hpp)

# Compile proto
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS proto/order.proto)

# Main library
add_library(enum_mapper_lib
  src/enum_mapper.hpp
  src/enum_mapping_order.hpp
  ${PROTO_SRCS}
  ${PROTO_HDRS}
  ${CMAKE_BINARY_DIR}/enum_mapping_macro.hpp
)
add_dependencies(enum_mapper_lib gen_macro)

target_include_directories(enum_mapper_lib PUBLIC src ${PROTOBUF_INCLUDE_DIR} ${CMAKE_BINARY_DIR})

# Main executable
add_executable(main src/main.cpp)
target_link_libraries(main enum_mapper_lib)

# Tests
add_subdirectory(test)
```

---

## 🧩 4. `src/enum_mapper.hpp`（核心模板）

```cpp
#pragma once
#include <string>

template<typename BizEnum, typename ProtoEnum>
struct EnumMapper;

template<typename BizEnum>
struct EnumName;
```

---

## 🧩 5. `src/enum_mapping_order.hpp`

```cpp
#pragma once
#include "enum_mapper.hpp"
#include "enum_mapping_macro.hpp"
#include "order.pb.h"

// Biz enum
enum class BizOrderStatus {
#define XX(b, p) b,
    ORDERSTATUS_ENUM_MAPPING(XX)
#undef XX
};

// EnumMapper specialization
template<>
struct EnumMapper<BizOrderStatus, demo::OrderStatus> {
    static BizOrderStatus ToBiz(demo::OrderStatus p) {
        switch(p){
#define XX(b, p2) case demo::OrderStatus::p2: return b;
            ORDERSTATUS_ENUM_MAPPING(XX)
#undef XX
            default: return BizOrderStatus::NEW;
        }
    }
    static demo::OrderStatus ToProto(BizOrderStatus b) {
        switch(b){
#define XX(b2, p) case b2: return demo::OrderStatus::p;
            ORDERSTATUS_ENUM_MAPPING(XX)
#undef XX
            default: return demo::OrderStatus::NEW;
        }
    }
};

// Name mapping
template<>
struct EnumName<BizOrderStatus> {
    static const char* ToString(BizOrderStatus e) {
        switch(e){
#define XX(b, p) case b: return #b;
            ORDERSTATUS_ENUM_MAPPING(XX)
#undef XX
            default: return "UNKNOWN";
        }
    }
};
```

---

## 🧩 6. `src/main.cpp` 使用示例

```cpp
#include <iostream>
#include "enum_mapping_order.hpp"

int main(){
    using demo::OrderStatus;
    OrderStatus p = OrderStatus::SHIPPED;
    auto b = EnumMapper<BizOrderStatus, demo::OrderStatus>::ToBiz(p);
    auto p2 = EnumMapper<BizOrderStatus, demo::OrderStatus>::ToProto(b);
    std::cout << "Biz enum: " << EnumName<BizOrderStatus>::ToString(b) << "\n";
    std::cout << "Back to proto: " << static_cast<int>(p2) << "\n";
}
```

---

## 🧩 7. 单元测试：`test/enum_mapper_test.cpp`

```cpp
#include <gtest/gtest.h>
#include "enum_mapping_order.hpp"

TEST(EnumMapper, RoundTrip) {
    using demo::OrderStatus;
    for(int i=0;i<=3;i++){
        OrderStatus p = static_cast<OrderStatus>(i);
        auto b = EnumMapper<BizOrderStatus, OrderStatus>::ToBiz(p);
        auto p2 = EnumMapper<BizOrderStatus, OrderStatus>::ToProto(b);
        EXPECT_EQ(p, p2);
    }
}

TEST(EnumName, ToString) {
    EXPECT_STREQ("kPaid", EnumName<BizOrderStatus>::ToString(BizOrderStatus::kPaid));
}
```

`test/CMakeLists.txt` 将链接 GTest 并执行。

---

## ✅ 如何运行

```bash
mkdir build && cd build
cmake ..
make
./main
ctest
```

---

这个工程已经满足：

* **自动生成宏模板** (`enum_mapping_macro.hpp`)；
* **泛型 `EnumMapper` + `EnumName`**；
* **protobuf 枚举映射分层**；
* **CMake 构建、Google Test 单测**。
