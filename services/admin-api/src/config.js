export const PORT = process.env.PORT || 24080;
export const MONGO_URL = process.env.MONGO_URL || 'mongodb://localhost:28017/silencer';
export const RABBITMQ_URL = process.env.RABBITMQ_URL || 'amqp://zsilencer:zsilencer@localhost:25672/';
export const JWT_SECRET = process.env.JWT_SECRET || 'changeme-in-production';
export const JWT_EXPIRES_IN = '8h';
export const LOBBY_PLAYER_AUTH_URL = process.env.LOBBY_PLAYER_AUTH_URL || 'http://localhost:15171';
