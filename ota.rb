require 'sinatra'

configure do
	set :port, 1884
end

set :bind, '0.0.0.0'

get '/' do
	logger.info("route /")
	"Hello World!"
end

get '/custom_ota' do
	logger.info("route custom ota")
	firmware_file = 'FIRMWARE/hello-world/0.2.2'
	logger.info("Sending file " + firmware_file)
	send_file firmware_file
end
