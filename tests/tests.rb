#
# Test harness for testing IOT firmware.
# Mostly various unit tests.
#

require "./iotstate.rb"
require "./timetest.rb"
require "./alarmstate.rb"
require "./outlet.rb"

$verbose = true
$debug = true

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
			c.publish(key, message, false)	# publish but do not retain
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
#t.register(AlarmState.new())
t.register(Outlet.new())

t.run()
