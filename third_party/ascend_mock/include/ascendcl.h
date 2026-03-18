// 极简模拟昇腾CANN核心接口
#ifndef ASCENDCL_H
#define ASCENDCL_H

typedef int aclError;
#define ACL_SUCCESS 0

aclError aclInit(const char* config) { return ACL_SUCCESS; }
aclError aclFinalize() { return ACL_SUCCESS; }

#endif // ASCENDCL_H