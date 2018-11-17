#
# Occasionally post the current time as an MQTT message
#

require 'mqtt'
#require 'timeout'
#require 'json'

@host = "localhost"

@interval = 60	# how often to post
@epoch = Time.new(2018,11,1).to_i

def publish(topic, value)
	puts "Publishing #{topic} <= #{value}" if @debug
	begin
		MQTT::Client.connect(@host) do |c|
			c.publish(topic, value, true)
		end

	rescue Exception => bang
		puts "Exception during Time publish"
		puts "Exception #{bang}"
		exit
	end
end

while true
	t = Time.new()
	publish("environment/IOTtime", t.to_i - @epoch)
	publish("devices/$broadcast/IOTtime", t.to_i - @epoch)
	sleep(@interval)
end
