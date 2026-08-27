export const setItem = (key: string, value: any) => {
  localStorage.setItem(key, value);
}

export const getItem = (key: string): any => {
  return localStorage.getItem(key);
}

export const setBool = (key: string, value: boolean) => {
  localStorage.setItem(key, String(value));
}

export const getBool = (key: string): boolean => {
  return localStorage.getItem(key) === 'true';
}

export const setObject = (key: string, value: object)=> {
  localStorage.setItem(key, JSON.stringify(value));
}

export const getObject = (key: string): any  | null=> {
  const item = localStorage.getItem(key);
  if(item == null || item.length < 1){
    return null;
  }
  return JSON.parse(item);
}

export const setNumber = (key: string, value: number) => {
  localStorage.setItem(key, value.toString());
}

export const getNumber = (key: string): number | null=> {
  const value = localStorage.getItem(key);
  return value ? Number(value) : null;
}

