

doc: version Doxyfile
	@doxygen > doxygen.log
	@echo "  DOXYGEN"

version: FORCE
	@$(SHELL_PATH) ../VERSION-GEN

.PHONY: FORCE doc
