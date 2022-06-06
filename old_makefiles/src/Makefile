all:
	+$(MAKE) -C common
	+$(MAKE) -C modules
	+$(MAKE) -C wrapper
	+$(MAKE) -C lib

install:
	+$(MAKE) install -C modules
	+$(MAKE) install -C wrapper
	+$(MAKE) install -C lib

clean:
	+$(MAKE) clean -C common
	+$(MAKE) clean -C modules
	+$(MAKE) clean -C wrapper
	+$(MAKE) clean -C lib
