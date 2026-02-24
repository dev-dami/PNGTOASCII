ROOT_DIR ?= ..
BIN ?= $(ROOT_DIR)/fib

.PHONY: run

run:
	mkdir -p output
	python3 scripts/generate_fixtures.py
	$(BIN) fixtures/white.png 4 4 output/white_png.txt >/dev/null
	cmp -s expected/white.txt output/white_png.txt
	$(BIN) fixtures/white.jpg 4 4 output/white_jpg.txt >/dev/null
	cmp -s expected/white.txt output/white_jpg.txt
	$(BIN) fixtures/stripes.png 4 2 output/stripes_png.txt >/dev/null
	cmp -s expected/stripes.txt output/stripes_png.txt
	FIB_BIN=$(BIN) python3 scripts/depth_edge_check.py
	@echo "all tests passed"
