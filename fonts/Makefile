BDF_FILE = mara-font-x16n.bdf
PCF_FILE = mara-font-x16n.pcf
FONTS_DIR = /usr/share/fonts/X11/misc

edit:
	gbdfed $(BDF_FILE)

build: $(BDF_FILE)
	bdftopcf -o $(PCF_FILE) $(BDF_FILE)
	gzip -9 $(PCF_FILE)

install: $(PCF_FILE).gz
	sudo mkdir -p $(FONTS_DIR)
	sudo cp $(PCF_FILE).gz $(FONTS_DIR)
	sudo mkfontdir $(FONTS_DIR)
	xset +fp $(FONTS_DIR)