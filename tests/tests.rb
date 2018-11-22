#
# Test harness for testing IOT firmware.
# Mostly various unit tests.
#

require "./iotstate.rb"
require "./timetest.rb"

$verbose = true
$debug = false

def passed(testname)
	print  "\033[1;32mPASSED\033[0m\t#{testname}";
end

def failed(testname)
	print  "\033[1;31mFAILED\033[0m\t#{testname}";
end

t = IOTState.new("localhost")
t.register(Timetest.new())

t.run()
