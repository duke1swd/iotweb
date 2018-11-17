#
# This bit of code pulls all of the persistent messages from
# Homie devices out of the mqtt server.
# It sets up a hash, keyed by device name.  Value of each device
# is a hash, keyed by top (& sub topic) with value of last received message.
#

require 'mqtt'
require 'timeout'

host = "localhost"
nlflag = false
c = MQTT::Client.connect(host)
c.subscribe('#')
while true
	begin
		Timeout::timeout(1) do
			begin
				topic,message = c.get()
			rescue Exception
				exit
			end
			if nlflag
				puts ""
				nlflag = false
			end
			if message.include?("firmware")
				puts "#{topic}: (suppresed)"
			else
				puts "#{topic}: #{message}"
			end
		end
	rescue Timeout::Error
		nlflag = true
		print "."
	end
end
