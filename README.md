[juson]: https://github.com/wgtdkp/juson
[nginx]: https://nginx.org/
[lighttpd]: https://www.lighttpd.net/

# julia [![Build Status](https://travis-ci.org/wgtdkp/julia.svg?branch=master)](https://travis-ci.org/wgtdkp/julia)

[中文](#中文)

A small yet high performance http server and reverse proxy. You may view it as a _tiny nginx_.

## Environment

* gcc 5.4.0
* linux 4.4.0

## Dependency

* _[juson]_

## Install

```shell
  $ git submodule update --remote --recursive
  $ cd src && make all
  $ make install # root required
```

## Run

```shell
$ julia # roo required, default listening at 8000
```

You may modify the config file to specify the port if there is a confliction.

## Debug

As julia run as a daemon, it is not convenient to debug.
Follow the steps to make it run in debug mode:

1. change the _INSTALL\_DIR_ in Makefile to your local repo, like:
  ```shell
  INSTALL_DIR = /home/foo/julia/ # the last slash required
  ```

2. turn on the _debug_ instruction in config.json
  ```json
  "debug": true,
  ```

## **Exciting**

**Making a http server and host your site is not extremely cool?**
**Then, what if your server is compiled by your own [compiler](https://github.com/wgtdkp/wgtcc)?**

Yes, i wrote a C11 compiler(let me call it [wgtcc](https://github.com/wgtdkp/wgtcc)). It compiles **julia**, and the site runs well :) You can install [wgtcc](https://github.com/wgtdkp/wgtcc), then compile **julia**:

```shell
  $ make CC=wgtcc all
```

It surprised me that the server compiled by **wgtcc** runs extremely fast!

have fun :)

## Todo

1. ~~fastcgi~~
2. chunked transform
3. benchmark

## Reference

1. _[nginx]_
2. _[lighttpd]_
3. _[juson]_


## 中文

**julia** 是一个短小精悍的高并发http服务器和反向代理。你可以将他想象成tiny nginx。我用它搭了[博客](http://www.wgtdkp.com/)。静态文件由julia serve, 动态内容pass到后端的[uwsgi](https://uwsgi-docs.readthedocs.io/en/latest/)(类似于fpm)。

[安装](#MAKE)
[调试](#DEBUG)
请参照上方。

最酷的是，这个服务器，包括正在运行的博客，都是我自己写的C编译器编译的。如果你对编译器也感兴趣，可以看[这里](https://www.github.com/wgtdkp/wgtcc)。按照其readme安装wgtcc后，你可以这样编译julia：
```shell
  $ make CC=wgtcc all
```

have fun :)

## 性能
benchmark: ab

均为8 worker, 1KiB静态页面

| 并发 | nginx | julia |
| ---- | ----- | ----- |
| 10   | 52K   | 54K   |
| 100  | 56K   | 56K   |
| 1000 | 46K   | 44K   |
