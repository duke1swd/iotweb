#
# Test harness for testing IOT firmware.
# Mostly various unit tests.
#

require "./iotstate.rb"
require "./timetest.rb"
require "./alarmstate.rb"

$verbose = true
$debug = false

def passed(testname)
	print  "\033[1;32mPASSED\033[0m\t#{testname}";
end

def failed(testname)
	print  "\033[1;31mFAILED\033[0m\t#{testname}";
end

def publish(key, message)
	m = (/firmware/ =~ key)? 'binary': message
	puts "Publishing #{key} <= #{m}" if $debug
	begin
		MQTT::Client.connect($host) do |c|
			c.publish(key, message, true)
		end

	rescue Exception => bang
		puts "Exception during publish"
		puts "Exception #{bang}"
		exit
	end
end

$host = "localhost"
t = IOTState.new($host)
#t.register(Timetest.new())
t.register(AlarmState.new())

t.run()
