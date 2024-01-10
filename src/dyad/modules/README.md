## DYAD module

DYAD component to transfer files from a producer server to a consumer client using Flux APIs

#### Prerequisite:
Build and install flux-core. Please see https://github.com/flux-framework/flux-core.


#### To build `dyad.so`:

```
export PKG_CONFIG_PATH=<FLUX_INSTALL_ROOT>/lib/pkgconfig
cd src/module
make
```

#### To build test cases:

```
cd src/module/test
make
```

#### To run the test:

Frist, start a flux instance

```
<FLUX_INSTALL_ROOT>/bin/flux start -s 1 -o,-S,log-filename=out
```
This will create a new shell in which you can communicate with the instance.


```
<FLUX_INSTALL_ROOT>/bin/flux module load ./dyad.so <PRODUCER_ROOT_DIRECTORY>
```
This will start the dyad module.

```
flux exec ./compute <FILE> <CONSUMER_ROOT_DIRECTORY>
```
This will fetch <FILE> from <PRODUCER_ROOT_DIRECTORY> and put it under <CONSUMER_ROOT_DIRECTORY>
or simply
```
make test
```
