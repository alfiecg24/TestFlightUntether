SOURCES=Server.c
OUTPUT=TestFlightServices
XCODE_SDK=-sdk iphoneos clang
XCODE_FLAGS=-dynamiclib -w
FRAMEWORKS=-framework CoreFoundation
IPA_NAME=TestFlight.ipa
OUTPUT_IPA=TestFlightUntether.ipa

ifeq ($(shell uname -s), Darwin)
	COMPILER=xcrun
else
	$(error This project is only for macOS)
endif

ifeq (, $(shell which xcrun))
	$(error This project requires Xcode Command Line Tools to be installed)
endif

ifeq (, $(shell which unzip))
	$(error This project requires unzip to be installed)
endif

ifeq (, $(shell which tar))
	$(error This project requires tar to be installed)
endif

ifeq (, $(wildcard $(IPA_NAME)))
	$(error This project requires an unencrypted TestFlight.ipa to be present)
endif

all: $(SOURCES)
	@$(COMPILER) $(XCODE_SDK) $(SOURCES) $(XCODE_FLAGS) -o $(OUTPUT)
	@unzip -q $(IPA_NAME)
	@mv $(OUTPUT) Payload/TestFlight.app/Frameworks/TestFlightServices.framework/TestFlightServices
	@/usr/libexec/PlistBuddy -c "Set :NSExtension:ASDTestFlightServiceExtensionServiceTime -1" Payload/TestFlight.app/PlugIns/TestFlightServiceExtension.appex/Info.plist
	@tar -czf $(OUTPUT_IPA) Payload
	@rm -rf Payload
	@echo "Build output at $(OUTPUT_IPA)"
