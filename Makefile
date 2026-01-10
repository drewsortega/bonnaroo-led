# LED Grid Simulator Makefile
# Run from the Bonnaroo directory

.PHONY: simulator run-simulator clean-simulator

# Build the simulator
simulator:
	@mkdir -p simulator/build
	@cd simulator/build && cmake .. && make

# Build and run the simulator
run-simulator: simulator
	@cd simulator/build && ./led_simulator --base-path "$(CURDIR)"

# Clean build files
clean-simulator:
	@rm -rf simulator/build
