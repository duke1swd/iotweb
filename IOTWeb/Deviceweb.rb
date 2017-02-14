#
# Simple web site to manage the Homie devices.
#

require 'bundler/setup'
Bundler.require(:default)

$host = "localhost"
$message = NIL

#
# This bit of code pulls all of the persistent messages from
# Homie devices out of the mqtt server.
# It sets up a hash, keyed by device name.  Value of each device
# is a hash, keyed by top (& sub topic) with value of last received message.
#
def listOfDevices()
	returnHash = Hash.new
	c = MQTT::Client.connect($host)
	c.subscribe('devices/#')
	begin
	while true do
		Timeout::timeout(0.5) do
		    topic,message = c.get()
		    #puts "#{topic}: #{message}"
		    # pull the device name out of the message topic
		    device = topic.sub(/devices\/([a-zA-Z0-9 ]+)\/.*/, '\1')
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

def eraseDevice(device)
	messages = listOfDevices()[device]
	if messages
		c = MQTT::Client.connect($host)
		messages.each do |topic,value|
			c.publish("devices/" + device + "/" + topic, "", true)
		end
		c.disconnect
	end
end


configure do
	set :port, 1885
end

# Listen on all interfaces, even in dev mode.
set :bind, '0.0.0.0'

#
# Main page
#
get '/' do
	logger.info("route /")
	m = $message
	$message = 'no message'
	erb :index, :locals => {:message => m}
end

#
# Device detail page
#
get '/detail/:device' do
	device=CGI.unescape(params['device'])
	logger.info("route /detail for device " + device);
	erb :device_detail, :locals => {:device => device}
end

#
# Erase all persistant messages left over from an offline device
#
get '/erase/:device' do
	device=CGI.unescape(params['device'])
	logger.info("route /erase for device " + device);
	eraseDevice(device)
	$message = "Device #{device} erased"
	redirect to('/')
end

#
# API interface
#
get '/devices.json' do
	logger.info("route /devices.json")
	JSON.generate(listOfDevices())
end
