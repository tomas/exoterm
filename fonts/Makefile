BDF_FILE = mara-font-x16n.bdf
PCF_FILE = mara-font-x16n.pcf

edit:
	gbdfed $(BDF_FILE)

build:
	bdftopcf -o $(PCF_FILE) $(BDF_FILE)
	gzip -9 $(PCF_FILE)