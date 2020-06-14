#pragma once
#include <assert.h>

//#define D3DSAFERELEASE(ptr) if((ptr)) (ptr)->Release()


// Assertions
#define ASSERT_HRESULT_SUCCESS(result) assert(!FAILED((result)))

#ifdef _DEBUG
#define SucceedOrCrash(result) assert(!FAILED((result)))
#else 
#define SucceedOrCrash(result) if(FAILED((result))) throw std::exception("Something fucking broke")
#endif