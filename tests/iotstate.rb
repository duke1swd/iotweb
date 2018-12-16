#
# This class defines an object which is the state of the IOT world.
# You are only supposed to have one of these (unless you are talking
# to two completely independent MQTT/IOT systems).
#
# Basic idea is to create it, and then call the run function with a
# method to call when things change.  The method is called with a pointer to this object.
#

require 'mqtt'
require 'timeout'

class IOTState

def initialize(host)
	@host = host
	@stateHash = Hash.new
	@registeredTests = Array.new
end

##
# every test must have a run() method.
# The run method returns a test state, which is one of these:
# :test_not_run	# we are not running this test for some reason
# :test_done	# the test has been completed
# :test_running	# keep calling this test
# :test_new	# internal state, test never called yet.

def register(test)
	@registeredTests.push({state: :test_new, test: test})
end

#
# call a test
def callTest()
	@registeredTests.each do |t| 
		if {test_new: true, test_running: true}[t[:state]]
			puts "Running test #{t[:test].name()}" if $verbose and t[:state] == :test_new
			if (t[:state] = t[:test].run(@stateHash)) == :test_running
				return false
			end
		end
	end
	return true
end


def run()
	c = MQTT::Client.connect(@host)
	startTime = Time.new().to_i + 2;
	c.subscribe('#')
	while true do
	    begin
		Timeout::timeout(5) do
			topic,message = c.get()
			puts "#{topic}: #{message}" if $debug

			# (don't) ignore any broadcast messages lurking about
			# next if /\$broadcast/ =~ topic

			# handle devices
			if /^devices/ =~ topic

				# pull the device name out of the message topic
				device = topic.sub(/devices\/([a-zA-Z0-9\-\$]+)\/.*/, '\1')
				if ! @stateHash[device]
					@stateHash[device] = Hash.new
				end

				# get the rest of the topic, including sub topics.
				deviceTopic = topic.sub(/devices\/[^\/]*\/(.*)/, '\1')
				@stateHash[device][deviceTopic] = message
				# puts "device = #{device}  deviceTopic = #{deviceTopic}  message = #{message}" if $debug
			end
		end
	    rescue Timeout::Error
			# Ignore the timeout error, just means we are not waiting for more.
			# This ensures the test gets called at last once per second.
	    end
	    # Don't do callouts for the first 2 seconds, wait until all persistent messages are loaded
	    if (Time.new().to_i > startTime)
		    break if callTest()
	    end
	end

	c.disconnect
end

end	# of the class
