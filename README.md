[juson]: https://github.com/wgtdkp/juson
[nginx]: https://nginx.org/
[lighttpd]: https://www.lighttpd.net/

# julia
A small yet high performance http server and reverse proxy

### ENVIRONMENT
* gcc 5.4.0
* linux 4.4.0

### DEPENDENCY
* _[juson]_

### MAKE
```shell
  $ git submodule update --init
  $ make all
```

### RUN
```
$ ./build/julia config.json
```
You may modify the config file to specify the port if there is a confliction.

### **EXICTING**
**Making a http server and host your site is not extremely cool?**

**Then, what if your server is compiled by your own [compiler](https://github.com/wgtdkp/wgtcc)?**

Yes, i wrote a C11 compiler(let me call it [wgtcc](https://github.com/wgtdkp/wgtcc)). It compiles **julia**, and the site runs well. :) You can install [wgtcc](https://github.com/wgtdkp/wgtcc), then compile **julia**:

```shell
  $ make CC=wgtcc all
```
It surprised me that the server compiled by **wgtcc** runs extremely fast!

### TODO
1. fastcgi
2. chunked transform
3. benchmark

### REFERENCE
1. _[juson]_
2. _[nginx]_
3. _[lighttpd]_
 