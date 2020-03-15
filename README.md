## 介绍
>* nginx里的数组是从内存池里分配的，主要储存在小内存块，而不是走大数据块策略。
>* 在创建数组的时候，每个元素大小已经被固定，后续不能更改

### 1.数据结构定义
#### **⑴ ngx_array_t**  数组的主要结构
含义具体看下面的注释和配图
```c
typedef struct {        // 数组定义
    void        *elts;  // 数组首地址
    ngx_uint_t   nelts; // 第一个未使用元素的索引(类似于栈顶指针)
    size_t       size;  // 每个元素所占空间大小(创建数组时已经固定)
    ngx_uint_t   nalloc;// 分配多少个元素
    ngx_pool_t  *pool;  // 内存池(在内存池上分配)
} ngx_array_t;
```

**数组结构：**
![ngx数组.jpg](https://upload-images.jianshu.io/upload_images/18154407-fb2824f0012d0128.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


### 2.函数实现
#### **⑴ 创建数组**
数组是在内存池上创建的（我的前一篇文章写了nginx的内存池）
```c
ngx_array_t *
ngx_array_create(ngx_pool_t *p, ngx_uint_t n, size_t size)  // 创建数组(在内存池上创建), n 元素个数, size 每个元素大小
{
    ngx_array_t *a;

    a = ngx_palloc(p, sizeof(ngx_array_t));  // 在内存池p上申请ngx_array_t大小的内存
    if (a == NULL) {
        return NULL;
    }

    // 第一次在pool上分配数组数据结构所需要的内存
    // 第二次在pool在上分配数组元素所需要的内存
    if (ngx_array_init(a, p, n, size) != NGX_OK) {  // ngx_array_init是内联函数，函数定义在头文件中ngx_array.h
        return NULL;
    }

    return a;  // 成功返回数组首地址
}
```

#### ngx_array_init() 的定义（在头文件中）
```c
// ngx_int_t 是 nginx 里自定义的类似，这里把它看做是 int 就可以了
static ngx_inline ngx_int_t
ngx_array_init(ngx_array_t *array, ngx_pool_t *pool, ngx_uint_t n, size_t size)
{
    /*
     * set "array->nelts" before "array->elts", otherwise MSVC thinks
     * that "array->nelts" may be used without having been initialized
     */
    // 上面是官方的注释：先初始化array->nelts，在出初始化array->nelts，否则MSVC认为array->nelts可以在没有初始化的情况下使用
    array->nelts = 0;
    array->size = size;
    array->nalloc = n;
    array->pool = pool;

    array->elts = ngx_palloc(pool, n * size);
    if (array->elts == NULL) {
        return NGX_ERROR;
    }

    return NGX_OK;
}
```

#### **⑵ 销毁数组**
* **ps: 大部分数据结构都是有创建、销毁、get操作、set操作等。万变不离其宗**
```c
// 数组末尾是指向p->d.last(不知道p->d.last的话，看上一篇内存池的分析)
void
ngx_array_destroy(ngx_array_t *a)
{
    ngx_pool_t  *p;

    p = a->pool;

    // 跟创建的顺序相反
    // 1. 先回收数组元素的内存
    if ((u_char *) a->elts + a->size * a->nalloc == p->d.last) {
        p->d.last -= a->size * a->nalloc;
    }

    // 2. 再回收数组数据结构所占用的内存
    if ((u_char *) a + sizeof(ngx_array_t) == p->d.last) {
        p->d.last = (u_char *) a;
    }
}
```

#### **⑶ 数据插入**
**这里注意的是：**
**① 数组容量不足以支持插入操作的时候，会申请多一倍的内存（原来是size大小，申请 2 * size 大小）**
**② 若当前内存池剩余容量不足以支持申请2 * size的操作，则会在另一块内存做扩容操作（也就是在另一个内存池中做扩容，并将原来的数据复制过去）**
```c
void *
ngx_array_push(ngx_array_t *a)
{
    void        *elt, *new;
    size_t       size;
    ngx_pool_t  *p;

    // 数组满了，需要扩容
    if (a->nelts == a->nalloc) {

        /* the array is full */

        size = a->size * a->nalloc;

        p = a->pool;

        if ((u_char *) a->elts + size == p->d.last
            && p->d.last + a->size <= p->d.end)
        {   // 数组的末尾是内存池可用地址的开头且剩余空间足够
            /*
             * the array allocation is the last in the pool
             * and there is space for new allocation
             */

            p->d.last += a->size;
            a->nalloc++;

        } else {  // 内存池剩余空间不足以扩容，需要在另一块内存上申请两倍的内存，并将数组指向新的地址
            /* allocate a new array */

            new = ngx_palloc(p, 2 * size);
            if (new == NULL) {
                return NULL;
            }

            ngx_memcpy(new, a->elts, size);
            a->elts = new;
            a->nalloc *= 2;
        }
    }

    elt = (u_char *) a->elts + a->size * a->nelts;
    a->nelts++;

    return elt;
}
```

#### **⑷ 数据插入（支持插入多个）**
这里不做过多解释了，和上面的差不多
```c
void *
ngx_array_push_n(ngx_array_t *a, ngx_uint_t n)   // 同上，支持多个元素
{
    void        *elt, *new;
    size_t       size;
    ngx_uint_t   nalloc;
    ngx_pool_t  *p;

    size = n * a->size;

    if (a->nelts + n > a->nalloc) {

        /* the array is full */

        p = a->pool;

        if ((u_char *) a->elts + a->size * a->nalloc == p->d.last
            && p->d.last + size <= p->d.end)
        {
            /*
             * the array allocation is the last in the pool
             * and there is space for new allocation
             */

            p->d.last += size;
            a->nalloc += n;

        } else {
            /* allocate a new array */

            nalloc = 2 * ((n >= a->nalloc) ? n : a->nalloc);

            new = ngx_palloc(p, nalloc * a->size);
            if (new == NULL) {
                return NULL;
            }

            ngx_memcpy(new, a->elts, a->nelts * a->size);
            a->elts = new;
            a->nalloc = nalloc;
        }
    }

    elt = (u_char *) a->elts + a->size * a->nelts;
    a->nelts += n;

    return elt;
}
```
---
2020.2.1  15:45  广州
