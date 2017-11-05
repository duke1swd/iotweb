#
# This bit of code pulls all of the persistent messages from
# Homie devices out of the mqtt server.
# It sets up a hash, keyed by device name.  Value of each device
# is a hash, keyed by top (& sub topic) with value of last received message.
#

require 'mqtt'
require 'timeout'

def listOfDevices()
	returnHash = Hash.new
	host = "localhost"
	c = MQTT::Client.connect(host)
	c.subscribe('devices/#')
	begin
	while true do
		Timeout::timeout(1) do
		    topic,message = c.get()
		    #puts "#{topic}: #{message}"
		    # pull the device name out of the message topic
		    device = topic.sub(/devices\/([a-zA-Z0-9\-]+)\/.*/, '\1')
		    if ! returnHash[device]
		    	returnHash[device] = Hash.new
		    end

		    # get the rest of the topic, including sub topics.
		    deviceTopic = topic.sub(/devices\/#{device}\/(.*)/, '\1')
		    returnHash[device][deviceTopic] = message
		end
	end
	rescue Timeout::Error
		# Ignore the timeout error, just means we are not waiting for more.
		# Mostly this is a hack to get the persistent messages only.
	end

	c.disconnect
	return returnHash
end

# debugging routine to print what we got from mqtt
if true
	listOfDevices.each do |device,devHash|
		puts device
		devHash.each do |topic,message|
			puts "\t#{topic}:  #{message}"
		end
	end
end
