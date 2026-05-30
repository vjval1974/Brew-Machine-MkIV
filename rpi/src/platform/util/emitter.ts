import { EventEmitter } from 'events';

// Single application-wide event bus. Controllers publish state changes here;
// the WebSocket server forwards them to connected browsers.
const bus = new EventEmitter();
bus.setMaxListeners(100);

export default bus;
