
serve: tea.js .dummy
	python3 -m http.server

tea.js: .dummy
	$(MAKE) -Bj -C ../src tea.js TEA_T=tea.js CC=emcc TARGET_STRIP=: TARGET_XSHLDFLAGS='-shared -s SIDE_MODULE=1' CFLAGS=-Os LDFLAGS='-Os -s MODULARIZE=1 -s EXPORT_ES6=1 -s EXPORTED_RUNTIME_METHODS=callMain -s SINGLE_FILE=1' Q=
	cp ../src/tea.js tea.js

.dummy:
