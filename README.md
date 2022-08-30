# Branchless UTF-8 Decoder

Full article:
[A Branchless UTF-8 Decoder](http://nullprogram.com/blog/2017/10/06/)

## Example usage

```c
#define N (1 << 20)  // 1 MiB

// input buffer with 3 bytes of zero padding
char buf[N+3];
char *end = buf + fread(buf, 1, N, stdin);
end[0] = end[1] = end[2] = 0;

// output buffer: parsed code points
int len = 0;
uint32_t cp[N];

int errors = 0;
for (char *p = buf; p < end;) {
    int e;
    p = utf8_decode(p, cp+len++, &e);
    errors |= e;
}
if (errors) {
    // decode failure
}
```
