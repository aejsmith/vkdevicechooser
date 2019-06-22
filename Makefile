all: libVkLayer_device_chooser.so

libVkLayer_device_chooser.so: layer.cpp
	g++ -gdwarf-2 -shared -O2 -fPIC -std=gnu++11 layer.cpp -o libVkLayer_device_chooser.so

install: libVkLayer_device_chooser.so
	mkdir -p "${HOME}/.local/share/vulkan/implicit_layer.d/"
	cp -v libVkLayer_device_chooser.so VkLayer_device_chooser.json "${HOME}/.local/share/vulkan/implicit_layer.d/"

clean:
	rm -f libVkLayer_device_chooser.so

.PHONY: all install clean
